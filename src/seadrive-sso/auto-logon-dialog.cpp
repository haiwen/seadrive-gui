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
// const char* kServerUrl = "http://stg-sharefile.paic.com.cn";
const char* kSeahubApiTokenCookieName = "seahub_auth";
#if defined(Q_OS_WIN32)
const char *const kSeafileConfigureFileName = "seafile.ini";
#else
const char *const kSeafileConfigureFileName = ".seafilerc";
#endif
const char *const kSeafilePreconfigureGroupName = "preconfigure";
const char *const kPreconfigureServerAddr = "PreconfigureServerAddr";
}

AutoLogonDialog::AutoLogonDialog(const QUrl& url, QWidget* parent)
    : QDialog(parent), login_url_(url)
{
    setWindowTitle(
        QString("%1 %2").arg(getBrand()).arg(QString::fromUtf8("自动登录")));
    setWindowIcon(QIcon(":/images/seafile.png"));
    QHBoxLayout* layout = new QHBoxLayout;

    QWidget* lspacer = new QWidget;
    lspacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    layout->addWidget(lspacer);

    QLabel* label = new QLabel(QString::fromUtf8("自动登录中， 请稍候"));
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

/**
* Read the server address from windows regsitry. The value shoud be
* preconfigured by AD.
*
*   key: HKEY_LOCAL_MACHINE\SOFTWARE\\SeaDrive\\ServerUrl
*   value: like http://sharefile.seafile.com.cn
**/
QUrl AutoLogonDialog::readServerUrlFromRegistry()
{
    QUrl url;
    RegElement reg(HKEY_LOCAL_MACHINE, "SOFTWARE\\SeaDrive", "ServerUrl", "");
    if (!reg.exists()) {
        qWarning("failed to read server url from registry");
        return url;
    }
    reg.read();
    url = reg.stringValue();
    if (!url.isValid()) {
        errorAndExit(
            QString::fromUtf8("服务器地址格式错误: %1").arg(reg.stringValue()));
    }
    return url;
}

QUrl AutoLogonDialog::readServerUrlFromConfigFile(const QString& key)
{
    QUrl url;
    QString configure_file = QDir::home().filePath(kSeafileConfigureFileName);
    if (!QFileInfo(configure_file).exists()) {
        qWarning("config file not exists");
        return url;
    }
    QSettings setting(configure_file, QSettings::IniFormat);
    setting.beginGroup(kSeafilePreconfigureGroupName);
    QVariant value = setting.value(key);
    setting.endGroup();
    url = value.toString();
    if (!url.isValid()) {
        errorAndExit(
            QString::fromUtf8("服务器地址格式错误：%1").arg(url.toString()));
    }
    return url;
}

QUrl AutoLogonDialog::askForServerUrl()
{
    QUrl url;
    QString str;
    bool ok;
    while (true) {
        str = QInputDialog::getText(this, QString::fromUtf8("请输入服务器地址"),
                                    QString::fromUtf8("地址"),
                                    QLineEdit::Normal, str, &ok)
                  .trimmed();
        if (!ok) {
            qWarning("user cancelled the server url input dialog.");
            errorAndExit();
            return QUrl();
        }
        if (str.isEmpty()) {
            warn(QString::fromUtf8("地址不能为空"));
            continue;
        }
        url = QUrl(str);
        if (!url.isValid()) {
            warn(QString::fromUtf8("错误的地址"));
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
    // then try to read it from registry
    if (!login_url_.isValid()) {
        qWarning("tring to read server url from registry.");
        login_url_ = readServerUrlFromRegistry();
        source = "registry";
    }
    // then try to read it from cofigure file
    if (!login_url_.isValid()) {
        qWarning("tring to read server url form configure file");
        login_url_ = readServerUrlFromConfigFile(kPreconfigureServerAddr);
        source = "configure file";
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
    login_url_.setPath("/sso");
    qWarning("auto logon to %s (source: %s)",
             login_url_.toString().toUtf8().data(),
             source.toUtf8().data());

    QHash<QString, QString> params = ::getSeafileLoginParams(
        gui->settingsManager()->getComputerName(), "krb5_");
    params["from_desktop"] = "true";

    WinInetHttpReqest request(::includeQueryParams(login_url_, params));
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
    return Account(login_url_, email, token);
}

void AutoLogonDialog::errorAndExit(const QString& msg)
{
    QString content = QString::fromUtf8("登录失败");
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
