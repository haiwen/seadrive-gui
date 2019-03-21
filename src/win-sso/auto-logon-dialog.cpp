#include <QtGlobal>
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtWidgets>
#else
#include <QtGui>
#endif
#include <glib.h>
#include <QTimer>

#include "utils/api-utils.h"
#include "utils/registry.h"
#include "utils/utils.h"
#include "win-http-request.h"

#include "account-mgr.h"
#include "seadrive-gui.h"
#include "settings-mgr.h"

#include "auto-logon-dialog.h"

namespace
{
const char* kSeahubApiTokenCookieName = "seahub_auth";
const char *const kPreconfigureServerAddr = "PreconfigureServerAddr";
}

AutoLogonDialog::AutoLogonDialog(const QUrl& url, QWidget* parent)
    : QDialog(parent), login_url_(url)
{
    setWindowTitle(
        QString("%1 %2").arg(getBrand()).arg(tr("auto login")));
    setWindowIcon(QIcon(":/images/seafile.png"));
    QHBoxLayout* layout = new QHBoxLayout;

    QWidget* lspacer = new QWidget;
    lspacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout->addWidget(lspacer);

    QLabel* label = new QLabel(tr("auto logining, please wait a moment"));
    layout->addWidget(label);

    QWidget* rspacer = new QWidget;
    rspacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout->addWidget(rspacer);

    setLayout(layout);


    setStyleSheet(
        "QDialog { min-width: 300px; max-wdith: 300px; min-height: 130px; "
        "max-height: 130px; }");

    QTimer::singleShot(100, this, SLOT(startAutoLogon()));
}

QUrl AutoLogonDialog::askForServerUrl()
{
    QUrl url;
    QString str;
    bool ok;
    while (true) {
        str = QInputDialog::getText(this, tr("Please input server address"),
                                    tr("address"),
                                    QLineEdit::Normal, str, &ok)
                  .trimmed();
        if (!ok) {
            qWarning("user cancelled the server url input dialog.");
            errorAndExit();
            return QUrl();
        }
        if (str.isEmpty()) {
            warn(tr("Please enter the server address"));
            continue;
        }
        url = QUrl(str);
        if (!url.isValid()) {
            warn(tr("%1 is not a valid server address").arg(str));
            continue;
        }
        return url;
    }
}

void AutoLogonDialog::startAutoLogon()
{
    QString source;
    // first try environment variable
    if (!login_url_.isValid()) {
        qWarning("tring to load server url from environment variable.");
        const char* env = g_getenv("SEAFILE_SERVER_URL");
        qWarning("SEAFILE_SERVER_URL = %s", env ? env : "null");
        if (env) {
            login_url_ = QUrl(QString::fromUtf8(env));
            source = "environment variable";
        }
    }
    // then try read it from registry or configure file
    if (!login_url_.isValid()) {
        qWarning("tring to read server url form registry or configure file");
        login_url_ = QUrl(gui->readPreconfigureEntry(kPreconfigureServerAddr).toString());
        source = "preconfigureentry";
    }
    // finally, ask the user to input the server address
    if (!login_url_.isValid()) {
        qWarning("asking the user for server url.");
        login_url_ = askForServerUrl();
        source = "user input";
    }
    if (!login_url_.isValid()) {
        reject();
        return;
    }
    QUrl sso_url(QString("%1/sso").arg(login_url_.toString()));
    qWarning("auto logon to %s (source: %s)",
             sso_url.toString().toUtf8().data(),
             source.toUtf8().data());

    QHash<QString, QString> params = ::getSeafileLoginParams(
        gui->settingsManager()->getComputerName(), "krb5_");
    params["from_desktop"] = "true";

    WinInetHttpReqest request(::includeQueryParams(sso_url, params));
    WinInetHttpResponse response;

    if (!request.send(&response)) {
        errorAndExit();
        return;
    }

    QString cookie_value;
    if (!request.getCookie(kSeahubApiTokenCookieName, &cookie_value)) {
        errorAndExit();
        return;
    }

    qDebug("cookie value is %s", cookie_value.toUtf8().data());

    Account account = parseAccount(cookie_value);
    if (!account.isValid()) {
        qWarning("the cookie returned by server is incorrect");
        errorAndExit();
        return;
    }

    qDebug("adding new autologon account, token is %s",
           account.token.toUtf8().data());

    if (gui->accountManager()->saveAccount(account) < 0) {
        gui->warningBox(tr("Failed to save current account"), this);
        reject();
    }
    else {
        accept();
    }
}

Account AutoLogonDialog::parseAccount(const QString& cookie_value)
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
    return Account(login_url_, email, token, 0, false, true, true);
}

void AutoLogonDialog::errorAndExit(const QString& msg)
{
    QString content = tr("login failed");
    if (!msg.isEmpty()) {
        content += "\n" + msg;
    }
    // gui->errorAndExit(content, this);
    reject();
}

void AutoLogonDialog::warn(const QString& msg)
{
    gui->warningBox(msg, this);
}
