#include <QtGui>
#if defined(SEAFILE_USE_WEBKIT)
  #include <QWebView>
#else
  #include <QWebEngineView>
  #include <QWebEnginePage>
  #include <QWebEngineProfile>
  #include <QWebEngineCookieStore>
  #include "shib-helper.h"
#endif
#include <QVBoxLayout>
#include <QList>
#include <QLineEdit>
#include <QSslError>
#include <QNetworkReply>
#include <QNetworkCookie>

#include "seadrive-gui.h"
#include "utils/utils.h"
#include "utils/api-utils.h"
#include "account-mgr.h"
#include "network-mgr.h"
#include "ui/init-sync-dialog.h"
#include "ui/sync-root-name-dialog.h"
#include "api/api-error.h"
#include "api/requests.h"

#include "shib-login-dialog.h"

namespace {

const char *kSeahubShibCookieName = "seahub_auth";

} // namespace

ShibLoginDialog::ShibLoginDialog(const QUrl& url,
                                 const QString& computer_name,
                                 QWidget *parent)
    : QDialog(parent),
      url_(url),
      cookie_seen_(false),
      account_info_req_(NULL)
{
    setWindowTitle(tr("Single Sign On"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    QVBoxLayout *vlayout = new QVBoxLayout();
    setLayout(vlayout);

    address_text_ = new QLineEdit;
    address_text_->setObjectName("addressText");
    address_text_->setText(url.toString());
    address_text_->setReadOnly(true);
    vlayout->addWidget(address_text_);

#if defined(SEAFILE_USE_WEBKIT)
    webview_ = new QWebView;
    CustomCookieJar *jar = new CustomCookieJar(this);
    QNetworkAccessManager *mgr = webview_->page()->networkAccessManager();
    NetworkManager::instance()->addWatch(mgr);

    mgr->setCookieJar(jar);

    connect(mgr, SIGNAL(sslErrors(QNetworkReply*, const QList<QSslError>&)),
            this, SLOT(sslErrorHandler(QNetworkReply*, const QList<QSslError>&)));

    connect(jar, SIGNAL(newCookieCreated(const QUrl&, const QNetworkCookie&)),
            this, SLOT(onNewCookieCreated(const QUrl&, const QNetworkCookie&)));
#else
    webview_ = new QWebEngineView;

    web_engine_profile_ = new QWebEngineProfile();
    web_engine_page_ = new QWebEnginePage(web_engine_profile_);

    webview_->setPage(web_engine_page_);
    QWebEngineCookieStore *jar = webview_->page()->profile()->cookieStore();
    connect(jar, SIGNAL(cookieAdded(const QNetworkCookie&)),
            this, SLOT(onWebEngineCookieAdded(const QNetworkCookie&)));
#endif

    QUrl shib_login_url(url_);
    QString path = shib_login_url.path();
    if (!path.endsWith("/")) {
        path += "/";
    }
    path += "shib-login";
    shib_login_url.setPath(path);

    connect(webview_, SIGNAL(urlChanged(const QUrl&)),
           this, SLOT(updateAddressBar(const QUrl&)));

    vlayout->addWidget(webview_);
    webview_->load(::includeQueryParams(
                       shib_login_url, ::getSeafileLoginParams(computer_name, "shib_")));
}

ShibLoginDialog::~ShibLoginDialog()
{

#if !defined(SEAFILE_USE_WEBKIT)
    // The web_engine_page_ object must delete before web_engine_profile.
    web_engine_page_->deleteLater();
    web_engine_profile_->deleteLater();
#endif
}

void ShibLoginDialog::sslErrorHandler(QNetworkReply* reply,
                                      const QList<QSslError> & ssl_errors)
{
    reply->ignoreSslErrors();
}

void ShibLoginDialog::onNewCookieCreated(const QUrl& url, const QNetworkCookie& cookie)
{
    if (cookie_seen_) {
        return;
    }
    QString name = cookie.name();
    QString value = cookie.value();
    if (url.host() == url_.host() && name == kSeahubShibCookieName) {
        account_ = parseAccount(value);
        if (!account_.isValid()) {
            qWarning("wrong account information from server");
            return;
        }
        cookie_seen_ = true;

        account_info_req_ = new FetchAccountInfoRequest(account_);
        connect(account_info_req_, SIGNAL(success(const AccountInfo&)), this,
                SLOT(onFetchAccountInfoSuccess(const AccountInfo&)));
        connect(account_info_req_, SIGNAL(failed(const ApiError&)), this,
                SLOT(onFetchAccountInfoFailed(const ApiError&)));
        account_info_req_->send();
    }
}

void ShibLoginDialog::onFetchAccountInfoSuccess(const AccountInfo& info)
{
    FetchAccountInfoRequest *request = account_info_req_;
    account_info_req_ = NULL;
    request->deleteLater();

    account_.username = info.email;
    account_.accountInfo = info;

#ifdef Q_OS_WIN32
    if (gui->accountManager()->getPreviousSyncRootName(account_).isEmpty()) {
        QString name = gui->readPreconfigureExpandedString(kPreconfigureCustomSyncRootName, QVariant(), true);

        if (name.isEmpty()) {
            name = gui->accountManager()->genSyncRootName(account_);

            SyncRootNameDialog dialog(name, this);
            if (!dialog.exec()) {
                cookie_seen_ = false;
                account_ = Account();
                return;
            }
            name = dialog.customName();
        }

        if (gui->accountManager()->isSyncRootNameUsed(name)) {
            gui->warningBox(tr("A sync root folder with this name already exists."), this);
            cookie_seen_ = false;
            account_ = Account();
            return;
        }

        gui->accountManager()->setSyncRootName(account_, name);
    }
#endif

    gui->accountManager()->enableAccount(account_);
    gui->accountManager()->updateAccountInfo(account_, info);
    gui->initSyncDialog()->markNewLogin();

#ifndef Q_OS_MAC
    gui->initSyncDialog()->launch(EMPTY_DOMAIN_ID);
#endif

    accept();
}

void ShibLoginDialog::onFetchAccountInfoFailed(const ApiError& error)
{
    FetchAccountInfoRequest *request = account_info_req_;
    account_info_req_ = NULL;
    request->deleteLater();
    cookie_seen_ = false;
    account_ = Account();
    gui->warningBox(tr("Unable to fetch account information: %1").arg(error.toString()), this);
}

void ShibLoginDialog::updateAddressBar(const QUrl& url)
{
    address_text_->setText(url.toString());
    // Scroll to the left most.
    address_text_->home(false);
}


/**
 * The cookie value is like seahub_shib="foo@test.com@bd8cc1138", where
 * foo@test.com is username and bd8cc1138 is api token"
 */
Account ShibLoginDialog::parseAccount(const QString& cookie_value)
{
    QString txt = cookie_value;
    if (txt.startsWith("\"")) {
        txt = txt.mid(1, txt.length() - 2);
    }
    int pos = txt.lastIndexOf("@");
    QString email = txt.left(pos);
    QString token = txt.right(txt.length() - pos - 1);
    if (email.isEmpty() or token.isEmpty()) {
        return Account();
    }
    Account account(url_, email, token, 0, true);
    account.normalizedServerUrl();

    return account;
}

void ShibLoginDialog::onWebEngineCookieAdded(const QNetworkCookie& cookie)
{
    // printf("cookie added: %s = %s\n", cookie.name().data(), cookie.value().data());
    if (cookie.name() == kSeahubShibCookieName) {
        onNewCookieCreated(url_, cookie);
    }
}


CustomCookieJar::CustomCookieJar(QObject *parent)
    : QNetworkCookieJar(parent)
{
}

bool CustomCookieJar::setCookiesFromUrl(const QList<QNetworkCookie>& cookies, const QUrl& url)
{
  if (QNetworkCookieJar::setCookiesFromUrl(cookies, url)) {
      foreach (const QNetworkCookie& cookie, cookies) {
          emit newCookieCreated(url, cookie);
      }
    return true;
  }

  return false;
}

#if !defined(SEAFILE_USE_WEBKIT)
// We create an off-the-record QWebEngineProfile here, because we don't want the
// cookie to be persisted (the seahub_auth cookie should be cleared each time
// the shib login dialog is called)
SeafileQWebEnginePage::SeafileQWebEnginePage(QObject *parent)
    : QWebEnginePage(new QWebEngineProfile(parent), parent)
{
}

bool SeafileQWebEnginePage::certificateError(
    const QWebEngineCertificateError &certificateError)
{
    return true;
}


#endif
