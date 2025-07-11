#include <QtGlobal>

#include <QtWidgets>
#include <QtNetwork>
#include <QInputDialog>
#include <QStringList>
#include <QSettings>

#include "settings-mgr.h"
#include "account-mgr.h"
#include "seadrive-gui.h"
#include "settings-mgr.h"
#include "api/api-error.h"
#include "api/requests.h"
#include "login-dialog.h"
#include "init-sync-dialog.h"
#include "utils/utils.h"
#include "shib/shib-login-dialog.h"
#include "api/sso-status.h"
#include "ui/sync-root-name-dialog.h"

namespace {

const char *kUsedServerAddresses = "UsedServerAddresses";
const char *const kPreconfigureServerAddr = "PreconfigureServerAddr";
const char *const kPreconfigureServerAddrOnly = "PreconfigureServerAddrOnly";
const char *const kPreconfigureShibbolethLoginUrl = "PreconfigureShibbolethLoginUrl";

const char *const kSeafileOTPHeader = "X-Seafile-OTP";
const char *const kSchemeHTTPS = "https";

QStringList getUsedServerAddresses()
{
    QSettings settings;
    settings.beginGroup(kUsedServerAddresses);
    QStringList retval = settings.value("main").toStringList();
    settings.endGroup();
    QString preconfigure_addr = gui->readPreconfigureExpandedString(kPreconfigureServerAddr);
    if (!preconfigure_addr.isEmpty() && !retval.contains(preconfigure_addr)) {
        retval.push_back(preconfigure_addr);
    }
    return retval;
}

void saveUsedServerAddresses(const QString &new_address)
{
    QSettings settings;
    settings.beginGroup(kUsedServerAddresses);
    QStringList list = settings.value("main").toStringList();
    // put the last used address to the front
    list.removeAll(new_address);
    list.insert(0, new_address);
    settings.setValue("main", list);
    settings.endGroup();
}

} // namespace

LoginDialog::LoginDialog(QWidget *parent) : QDialog(parent)
{
    setupUi(this);
    setWindowTitle(tr("Add an account"));
    setWindowIcon(QIcon(":/images/seafile.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    request_ = NULL;
    account_info_req_ = NULL;
    connect(&client_sso_timer_, SIGNAL(timeout()),
            this, SLOT(checkClientSSOStatus()));

    mStatusText->setText("");
    mLogo->setPixmap(QPixmap(":/images/seafile-32.png"));
    QString preconfigure_addr = gui->readPreconfigureExpandedString(kPreconfigureServerAddr);
    if (gui->readPreconfigureEntry(kPreconfigureServerAddrOnly).toBool() && !preconfigure_addr.isEmpty()) {
        mServerAddr->setMaxCount(1);
        mServerAddr->insertItem(0, preconfigure_addr);
        mServerAddr->setCurrentIndex(0);
        mServerAddr->setEditable(false);
    } else {
        mServerAddr->addItems(getUsedServerAddresses());
        mServerAddr->clearEditText();
    }
    mServerAddr->setCompleter(nullptr);

    QString computerName = gui->settingsManager()->getComputerName();

    mComputerName->setText(computerName);

    mAutomaticLogin->setCheckState(Qt::Checked);

    connect(mSubmitBtn, SIGNAL(clicked()), this, SLOT(doLogin()));

    QRect screen;
    if (!QGuiApplication::screens().isEmpty()) {
        screen = QGuiApplication::screens().at(0)->availableGeometry();
    }
    move(screen.center() - this->rect().center());

    setupShibLoginLink();
}

void LoginDialog::setupShibLoginLink()
{
    connect(mSSOBtn, SIGNAL(clicked()),
            this, SLOT(loginWithShib()));
}

void LoginDialog::initFromAccount(const Account& account)
{
    setWindowTitle(tr("Re-login"));
    mTitle->setText(tr("Re-login"));
    mServerAddr->setMaxCount(1);
    mServerAddr->insertItem(0, account.serverUrl.toString());
    mServerAddr->setCurrentIndex(0);
    mServerAddr->setEditable(false);

    mUsername->setText(account.username);
    mPassword->setFocus(Qt::OtherFocusReason);
}

void LoginDialog::doLogin()
{
    if (!validateInputs()) {
        return;
    }
    saveUsedServerAddresses(url_.toString());

    mStatusText->setText(tr("Logging in..."));

    disableInputs();

    if (request_) {
        delete request_;
    }

    request_ = new LoginRequest(url_, username_, password_, computer_name_);
    if (!two_factor_auth_token_.isEmpty()) {
        request_->setHeader(kSeafileOTPHeader, two_factor_auth_token_);
    }

    connect(request_, SIGNAL(success(const QString&)),
            this, SLOT(loginSuccess(const QString&)));

    connect(request_, SIGNAL(failed(const ApiError&)),
            this, SLOT(loginFailed(const ApiError&)));

    request_->send();
}

void LoginDialog::disableInputs()
{
    mServerAddr->setEnabled(false);
    mUsername->setEnabled(false);
    mPassword->setEnabled(false);
    mSubmitBtn->setEnabled(false);
    mComputerName->setEnabled(false);
    mSSOBtn->setEnabled(false);
}

void LoginDialog::enableInputs()
{
    mServerAddr->setEnabled(true);
    mUsername->setEnabled(true);
    mPassword->setEnabled(true);
    mSubmitBtn->setEnabled(true);
    mComputerName->setEnabled(true);
    mSSOBtn->setEnabled(true);
}

void LoginDialog::onNetworkError(const QNetworkReply::NetworkError& error, const QString& error_string)
{
    showWarning(tr("Network Error:\n %1").arg(error_string));
    enableInputs();

    mStatusText->setText("");
}

void LoginDialog::onSslErrors(QNetworkReply* reply, const QList<QSslError>& errors)
{
    const QSslCertificate &cert = reply->sslConfiguration().peerCertificate();
    qDebug() << "\n= SslErrors =\n" << dumpSslErrors(errors);
    qDebug() << "\n= Certificate =\n" << dumpCertificate(cert);

    if (gui->detailedYesOrNoBox(tr("<b>Warning:</b> The ssl certificate of this server is not trusted, proceed anyway?"),
                                   dumpSslErrors(errors) + dumpCertificate(cert),
                                   this,
                                   false))
        reply->ignoreSslErrors();
}

bool LoginDialog::validateInputs()
{
    QString serverAddr = mServerAddr->currentText();
    QString protocol;
    QUrl url;

    if (serverAddr.size() == 0) {
        showWarning(tr("Please enter the server address"));
        return false;
    } else {
        if (!serverAddr.startsWith("http://") && !serverAddr.startsWith("https://")) {
            showWarning(tr("%1 is not a valid server address").arg(serverAddr));
            return false;
        }

        if (serverAddr.endsWith("/")) {
            serverAddr = serverAddr.left(serverAddr.size() - 1);
        }

        url = QUrl(serverAddr, QUrl::StrictMode);
        if (!url.isValid()) {
            showWarning(tr("%1 is not a valid server address").arg(serverAddr));
            return false;
        }
    }

    QString email = mUsername->text();
    if (email.size() == 0) {
        showWarning(tr("Please enter the username"));
        return false;
    }

    if (mPassword->text().size() == 0) {
        showWarning(tr("Please enter the password"));
        return false;
    }

    QString computer_name = mComputerName->text().trimmed();
    if (computer_name.size() == 0) {
        showWarning(tr("Please enter the computer name"));
    }

    url_ = url;
    username_ = mUsername->text();
    password_ = mPassword->text();
    computer_name_ = mComputerName->text();

    gui->settingsManager()->setComputerName(computer_name_);

    return true;
}

void LoginDialog::loginSuccess(const QString& token)
{
    // Some server configures mandatory http -> https redirect. In
    // such cases, we must update the server url to use https,
    // otherwise libcurl (used by the daemon) would be have trouble
    // dealing with it.
    if (url_.scheme() != kSchemeHTTPS && request_->reply()->url().scheme() == kSchemeHTTPS) {
        qWarning("Detected server %s redirects to https", toCStr(url_.toString()));
        url_.setScheme(kSchemeHTTPS);
    }
    if (account_info_req_) {
        account_info_req_->deleteLater();
    }
    account_info_req_ =
        new FetchAccountInfoRequest(Account(url_, username_, token));
    connect(account_info_req_, SIGNAL(success(const AccountInfo&)), this,
            SLOT(onFetchAccountInfoSuccess(const AccountInfo&)));
    connect(account_info_req_, SIGNAL(failed(const ApiError&)), this,
            SLOT(onFetchAccountInfoFailed(const ApiError&)));
    account_info_req_->send();
}

void LoginDialog::onFetchAccountInfoFailed(const ApiError& error)
{
    loginFailed(error);
}

void LoginDialog::loginFailed(const ApiError& error)
{
    switch (error.type()) {
    case ApiError::SSL_ERROR:
        onSslErrors(error.sslReply(), error.sslErrors());
        break;
    case ApiError::NETWORK_ERROR:
        onNetworkError(error.networkError(), error.networkErrorString());
        break;
    case ApiError::HTTP_ERROR:
        onHttpError(error.httpErrorCode());
    default:
        // impossible
        break;
    }
}

void LoginDialog::onFetchAccountInfoSuccess(const AccountInfo& info)
{
    Account account = account_info_req_->account();
    // The user may use the username to login, but we need to store the email
    // to account database
    account.username = info.email;
    account.isAutomaticLogin = mAutomaticLogin->checkState() == Qt::Checked;

    // The genSyncRootName() function retrieves the account info, but updateAccountInfo() should be executed after enableAccount(), which should follow setSyncRootName(). As a result, we will update the account info in advance.
    account.accountInfo = info;

#ifdef Q_OS_WIN32
    if (gui->accountManager()->getPreviousSyncRootName(account).isEmpty()) {
        QString name = gui->accountManager()->genSyncRootName(account);

        SyncRootNameDialog dialog(name, this);
        if (!dialog.exec()) {
            mStatusText->setText("");
            enableInputs();
            return;
        }

        gui->accountManager()->setSyncRootName(account, dialog.customName());
    }
#endif

    gui->accountManager()->enableAccount(account);
    gui->accountManager()->updateAccountInfo(account, info);

    gui->initSyncDialog()->markNewLogin();

#ifndef Q_OS_MAC
    gui->initSyncDialog()->launch(EMPTY_DOMAIN_ID);
#endif

    accept();
}

void LoginDialog::onHttpError(int code)
{
    const QNetworkReply* reply = request_->reply();
    if (reply->hasRawHeader(kSeafileOTPHeader) &&
        QString(reply->rawHeader(kSeafileOTPHeader)) == "required") {
        two_factor_auth_token_ = QInputDialog::getText(
            this,
            tr("Two Factor Authentication"),
            tr("Enter the two factor authentication token"),
            QLineEdit::Normal,
            "");
        if (!two_factor_auth_token_.isEmpty()) {
            doLogin();
            return;
        }
    } else {
        QString err_msg, reason;
        if (code == 400) {
            reason = tr("Incorrect email or password");
        } else if (code == 429) {
            reason = tr("Logging in too frequently, please wait a minute");
        } else if (code == 500) {
            reason = tr("Internal Server Error");
        }

        if (reason.length() > 0) {
            err_msg = tr("Failed to login: %1").arg(reason);
        } else {
            err_msg = tr("Failed to login");
        }

        showWarning(err_msg);
    }

    enableInputs();

    mStatusText->setText("");
}

void LoginDialog::showWarning(const QString& msg)
{
    gui->warningBox(msg, this);
}

void LoginDialog::loginWithShib()
{
    QString server_addr =
        gui->readPreconfigureEntry(kPreconfigureShibbolethLoginUrl)
        .toString()
        .trimmed();

    if (!server_addr.isEmpty()) {
        if (QUrl(server_addr).isValid()) {
            qWarning("using preconfigured shibboleth login url: %s\n",
                        toCStr(server_addr));
        } else {
            qWarning("invalid preconfigured shibboleth login url: %s\n",
                        toCStr(server_addr));
            server_addr = "";
        }
    }

    if (server_addr.isEmpty()) {
        server_addr = gui->settingsManager()->getLastShibUrl();
    }

    QString brand = getBrand() == "SeaDrive" ? "Seafile" : getBrand();
    server_addr = QInputDialog::getText(this, tr("Single Sign On"),
                                       tr("%1 Server Address").arg(brand),
                                       QLineEdit::Normal,
                                       server_addr);
    server_addr = server_addr.trimmed();
    if (server_addr.isEmpty()) {
        return;
    }

    if (!server_addr.startsWith("http://") && !server_addr.startsWith("https://")) {
        showWarning(tr("%1 is not a valid server address").arg(server_addr));
        return;
    }

    QUrl url = QUrl(server_addr, QUrl::StrictMode);
    if (!url.isValid()) {
        showWarning(tr("%1 is not a valid server address").arg(server_addr));
        return;
    }

    gui->settingsManager()->setLastShibUrl(server_addr);
    sso_server_ = url;

    ServerInfoRequest *request = new ServerInfoRequest(sso_server_);
    connect(request, SIGNAL(success(const ServerInfo&)),
            this, SLOT(serverInfoSuccess(const ServerInfo&)));
    connect(request, SIGNAL(failed(const ApiError&)),
            this, SLOT(serverInfoFailed(const ApiError&)));
    request->send();
}

void LoginDialog::serverInfoSuccess(const ServerInfo& info)
{
    if (!info.clientSSOViaLocalBrowser) {
        ShibLoginDialog shib_dialog(sso_server_, mComputerName->text(), this);
        if (shib_dialog.exec() == QDialog::Accepted) {
            accept();
        }
        return;
    }

    qInfo() << "begin sso login via local browser";

    ClientSSOLinkRequest *request = new ClientSSOLinkRequest(sso_server_);
    connect(request, SIGNAL(success(const QString&)),
            this, SLOT(clientSSOLinkSuccess(const QString&)));
    connect(request, SIGNAL(failed(const ApiError&)),
            this, SLOT(clientSSOLinkFailed(const ApiError&)));
    request->send();
}

void LoginDialog::serverInfoFailed(const ApiError& error)
{
    showWarning(tr("Failed to get server info. Please check the server address."));
}

void LoginDialog::clientSSOLinkSuccess(const QString& link)
{
    QString sso_link(link);

    qInfo() << "open sso link in browser:" << sso_link;
    openUrl(QUrl(sso_link));

    QRegularExpression re("/client-sso/([^/]+)");
    QRegularExpressionMatch match = re.match(sso_link);
    if (!match.hasMatch()) {
        qWarning() << "failed to parse token from client sso link" << sso_link;
        return;
    }

    QString token = match.captured(1);
    if (token.isEmpty()) {
        qWarning() << "failed to parse token from client sso link" << sso_link;
        return;
    }
    sso_token_ = token;

    client_sso_timer_.start(3000);
    client_sso_success_ = false;
}

void LoginDialog::clientSSOLinkFailed(const ApiError& error)
{
    showWarning(tr("Failed to get client sso link."));
}

void LoginDialog::checkClientSSOStatus()
{
    ClientSSOStatusRequest *request = new ClientSSOStatusRequest(sso_server_, sso_token_);
    connect(request, SIGNAL(success(const ClientSSOStatus&)),
            this, SLOT(clientSSOStatusSuccess(const ClientSSOStatus&)));
    connect(request, SIGNAL(failed(const ApiError&)),
            this, SLOT(clientSSOStatusFailed(const ApiError&)));
    request->send();
}

void LoginDialog::clientSSOStatusSuccess(const ClientSSOStatus& status)
{
    if (status.status == "waiting") {
        qInfo() << "waiting for client sso login";
        return;
    } else if (status.status != "success") {
        qWarning() << "client sso login failed:" << status.status;
        showWarning(tr("SSO login failed."));
        client_sso_timer_.stop();
        return;
    }
    client_sso_timer_.stop();

    // avoid multiple success signals
    if (client_sso_success_) {
        return;
    }
    client_sso_success_ = true;

    qInfo() << "client sso login success" << status.username;

    Account account(sso_server_, status.username, status.api_token);
    account_info_req_ = new FetchAccountInfoRequest(account);
    connect(account_info_req_, SIGNAL(success(const AccountInfo&)),
            this, SLOT(onFetchAccountInfoSuccess(const AccountInfo&)));
    connect(account_info_req_, SIGNAL(failed(const ApiError&)),
            this, SLOT(onFetchAccountInfoFailed(const ApiError&)));
    account_info_req_->send();
}

void LoginDialog::clientSSOStatusFailed(const ApiError& error)
{
    showWarning(tr("Failed to get client sso status."));
    client_sso_timer_.stop();
}
