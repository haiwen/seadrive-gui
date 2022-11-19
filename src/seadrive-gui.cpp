#include <QtGlobal>

#include <QtWidgets>
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QCoreApplication>
#include <QMessageBox>
#include <QHostInfo>
#include <QCryptoGraphicHash>

#include <errno.h>
#include <glib.h>

#include "utils/utils.h"
#include "utils/file-utils.h"
#include "utils/log.h"
#include "ui/tray-icon.h"
#include "ui/login-dialog.h"
#include "win-sso/auto-logon-dialog.h"
#include "ui/settings-dialog.h"
#include "ui/about-dialog.h"
#include "daemon-mgr.h"
#include "rpc/rpc-client.h"
#include "account-mgr.h"
#include "settings-mgr.h"
#include "message-poller.h"
#include "remote-wipe-service.h"
#include "account-info-service.h"
#include "file-provider-mgr.h"

#if defined(Q_OS_WIN32)
#include "utils/registry.h"
#include "utils/utils-win.h"
#include "ext-handler.h"
#include "ui/disk-letter-dialog.h"
#include "ui/seadrive-root-dialog.h"
#endif

#if defined(Q_OS_MAC)
#include "utils/utils-mac.h"
#endif

#ifdef HAVE_SPARKLE_SUPPORT
#include "auto-update-service.h"
#endif

#include "seadrive-gui.h"

namespace {

#if defined(Q_OS_MAC)
    const char *kSeadriveDirName = "Library/Containers/com.seafile.seadrive.fprovider/Data/Documents";
#elif defined(Q_OS_WIN32)
    const char *kPreconfigureCacheDirectory = "PreconfigureCacheDirectory";
    const char *kSeadriveDirName = "seadrive";
#else
    const char *kSeadriveDirName = ".seadrive";
#endif

const int kConnectDaemonIntervalMsec = 2000;

enum DEBUG_LEVEL {
  DEBUG = 0,
  WARNING
};

// -DQT_NO_DEBUG is used with cmake and qmake if it is a release build
// if it is debug build, use DEBUG level as default
#if !defined(QT_NO_DEBUG) || !defined(NDEBUG)
DEBUG_LEVEL seafile_client_debug_level = DEBUG;
#else
// if it is release build, use WARNING level as default
DEBUG_LEVEL seafile_client_debug_level = WARNING;
#endif

void myLogHandlerDebug(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
// Note: By default, this information (QMessageLogContext) is recorded only in debug builds.
// You can overwrite this explicitly by defining QT_MESSAGELOGCONTEXT or QT_NO_MESSAGELOGCONTEXT.
// from http://doc.qt.io/qt-5/qmessagelogcontext.html
#ifdef QT_MESSAGELOGCONTEXT
    case QtDebugMsg:
        g_debug("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtInfoMsg:
        g_info("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtWarningMsg:
        g_warning("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtCriticalMsg:
        g_critical("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtFatalMsg:
        g_critical("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        abort();
#else // QT_MESSAGELOGCONTEXT
    case QtDebugMsg:
        g_debug("%s\n", localMsg.constData());
        break;
    case QtInfoMsg:
        g_info("%s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        g_warning("%s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        g_critical("%s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        g_critical("%s\n", localMsg.constData());
        abort();
#endif // QT_MESSAGELOGCONTEXT
    default:
        break;
    }
}
void myLogHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
#ifdef QT_MESSAGELOGCONTEXT
    case QtInfoMsg:
        g_info("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtWarningMsg:
        g_warning("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtCriticalMsg:
        g_critical("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
    case QtFatalMsg:
        g_critical("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        abort();
#else // QT_MESSAGELOGCONTEXT
    case QtInfoMsg:
        g_info("%s\n", localMsg.constData());
        break;
    case QtWarningMsg:
        g_warning("%s\n", localMsg.constData());
        break;
    case QtCriticalMsg:
        g_critical("%s\n", localMsg.constData());
        break;
    case QtFatalMsg:
        g_critical("%s\n", localMsg.constData());
        abort();
#endif // QT_MESSAGELOGCONTEXT
    default:
        break;
    }
}

bool debugEnabledInDebugFlagFile()
{
    QFile debugflag_file(QDir::home().filePath("seafile-client-debug.txt"));
    if (!debugflag_file.exists()) {
        return false;
    }
    if (!debugflag_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream input(&debugflag_file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    input.setEncoding(QStringConverter::Utf8);
#else
    input.setCodec("UTF-8");
#endif

    QString debug_level = input.readLine().trimmed();
    return !debug_level.isEmpty() && debug_level != "false" &&
           debug_level != "0";
}


#ifdef Q_OS_MAC
void writeCABundleForCurl()
{
    QString dir = gui->seadriveDir();
    QString ca_bundle_path = pathJoin(dir, "ca-bundle.pem");
    QFile bundle(ca_bundle_path);
    if (bundle.exists()) {
        bundle.remove();
    }
    bundle.open(QIODevice::WriteOnly);
    const std::vector<QByteArray> certs = utils::mac::getSystemCaCertificates();
    for (size_t i = 0; i < certs.size(); i++) {
        QList<QSslCertificate> list = QSslCertificate::fromData(certs[i], QSsl::Der);
        foreach (const QSslCertificate& cert, list) {
            bundle.write(cert.toPem());
        }
    }
}
#endif

const char *const kPreconfigureUsername = "PreconfigureUsername";
const char *const kPreconfigureUserToken = "PreconfigureUserToken";
const char *const kPreconfigureServerAddr = "PreconfigureServerAddr";
const char *const kPreconfigureComputerName = "PreconfigureComputerName";
const char* const kHideConfigurationWizard = "HideConfigurationWizard";

#if defined(Q_OS_WIN32)
const char* const kPreconfigureUseKerberosLogin = "PreconfigureUseKerberosLogin";
const char *const kSeafileConfigureFileName = "seafile.ini";
const char *const kSeafileConfigurePath = "SOFTWARE\\Seafile";
const int kIntervalBeforeShowInitVirtualDialog = 3000;
#else
const char *const kSeafileConfigureFileName = ".seafilerc";
#endif
const char *const kSeafilePreconfigureGroupName = "preconfigure";

} // namespace


SeadriveGui *gui;

SeadriveGui::SeadriveGui(bool dev_mode)
    : dev_mode_(dev_mode),
      started_(false),
      in_exit_(false),
      first_use_(false)
{
    startup_time_ = QDateTime::currentMSecsSinceEpoch();
    tray_icon_ = new SeafileTrayIcon(this);
    daemon_mgr_ = new DaemonManager();
    rpc_client_ = new SeafileRpcClient();
    account_mgr_ = new AccountManager();
    settings_mgr_ = new SettingsManager();
    settings_dlg_ = new SettingsDialog();
    about_dlg_ = new AboutDialog();
    message_poller_ = new MessagePoller();

#if defined(Q_OS_MAC)
    file_provider_mgr_ = new FileProviderManager();
#endif

    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));
}

SeadriveGui::~SeadriveGui()
{
#ifdef HAVE_SPARKLE_SUPPORT
    AutoUpdateService::instance()->stop();
#endif

    delete tray_icon_;
    delete daemon_mgr_;
    delete rpc_client_;
    delete account_mgr_;
    delete message_poller_;

#if defined(Q_OS_MAC)
    delete file_provider_mgr_;
#endif

}

void SeadriveGui::start()
{
    started_ = true;

    if (!initLog()) {
        return;
    }

    qDebug("client id is %s", toCStr(getUniqueClientId()));

    // auto update rpc server start
    SeaDriveRpcServer::instance()->start();

    refreshQss();

    qWarning("seadrive gui started");

    account_mgr_->start();

    // Load system proxy information. This must be done before we start seadrive
    // daemon.
    QUrl url;
    if (!account_mgr_->accounts().empty()) {
        url = account_mgr_->accounts().front().serverUrl;
    }
    settings_mgr_->writeSystemProxyInfo(
        url, QDir(seadriveDataDir()).filePath("system-proxy.txt"));

#if defined(Q_OS_WIN32)
    QString preconfig_cache_dir = gui->readPreconfigureExpandedString(kPreconfigureCacheDirectory);
    if (!preconfig_cache_dir.isEmpty()) {
        QString prev_seadrive_root;
        if (settings_mgr_->getSeadriveRoot(&prev_seadrive_root)) {
            if (prev_seadrive_root.toLower() != preconfig_cache_dir.toLower()) {
               RegElement::removeIconRegItem();
            }
        }

        QDir cache_dir;
        if (!cache_dir.exists(preconfig_cache_dir)) {
            bool ok = cache_dir.mkdir(preconfig_cache_dir);
            if (!ok) {
                errorAndExit(tr("Failed to create seadrive cache directory"));
            }
        }
        seadrive_root_= preconfig_cache_dir;
    } else {
        QString seadrive_root;
        if (settings_mgr_->getSeadriveRoot(&seadrive_root)) {
            seadrive_root_= seadrive_root;
        } else {
            qWarning("cache directory not set, asking the user for it");

            SeaDriveRootDialog dialog;
            if (dialog.exec() != QDialog::Accepted) {
                errorAndExit(tr("Faild to choose a cache directory"));
                return;
            }

            seadrive_root_= dialog.seaDriveRoot();
        }
    }

    settings_mgr_->setSeadriveRoot(seadrive_root_);
    qWarning("Using cache directory: %s", toCStr(seadrive_root_));

    settings_mgr_->loadProxySettings();
    settings_mgr_->applyProxySettings();

    loginAccounts();

    connect(daemon_mgr_, SIGNAL(daemonStarted()),
            this, SLOT(onDaemonStarted()));
    connect(daemon_mgr_, SIGNAL(daemonRestarted()),
            this, SLOT(onDaemonRestarted()));
    daemon_mgr_->startSeadriveDaemon();

#elif defined(Q_OS_MAC)
    writeCABundleForCurl();

    settings_mgr_->loadProxySettings();
    settings_mgr_->applyProxySettings();

    loginAccounts();

    // The life cycle of seadrive daemon is managed by OS on mac, the
    // seadrive-gui has to wait until connect succeed.
    connect(&connect_daemon_timer_, SIGNAL(timeout()),
            this, SLOT(connectDaemon()));
    connect_daemon_timer_.start(kConnectDaemonIntervalMsec);
#endif
}

void SeadriveGui::loginAccounts()
{
    tray_icon_->show();

    if (first_use_ || account_mgr_->accounts().size() == 0) {
        do {
            QString username = readPreconfigureExpandedString(kPreconfigureUsername);
            QString token = readPreconfigureExpandedString(kPreconfigureUserToken);
            QString url = readPreconfigureExpandedString(kPreconfigureServerAddr);
            QString computer_name = readPreconfigureExpandedString(kPreconfigureComputerName, settingsManager()->getComputerName());
            bool is_use_kerberos_login = false;
#if defined(Q_OS_WIN32)
            QVariant use_kerberos_login = readPreconfigureExpandedString(kPreconfigureUseKerberosLogin, "0");
            is_use_kerberos_login = use_kerberos_login.toBool();
#endif

            if (!computer_name.isEmpty())
                settingsManager()->setComputerName(computer_name);
            if (!username.isEmpty() && !token.isEmpty() && !url.isEmpty()) {
                Account account(url, username, token);
                account_mgr_->enableAccount(account);
                break;
            }

            if (readPreconfigureEntry(kHideConfigurationWizard).toInt())
                break;

            if (!is_use_kerberos_login) {
                // A bug that changed default button styles is fixed here by
                // delaying the dialog 10ms.
                QTimer::singleShot(10, tray_icon_, SLOT(showLoginDialog()));
            } else {
#if defined(Q_OS_WIN32)
                AutoLogonDialog dialog;
                if (dialog.exec() != QDialog::Accepted) {
                    qWarning("auto logon failed, fall back to manual login");
                    warningBox(tr("Auto logon failed, fall back to manual login"));
                }
#endif
            }
        } while (0);
    } else {
        account_mgr_->validateAndUseAccounts();
    }
}

void SeadriveGui::logoutAccountsFromDaemon()
{
    if (!rpc_client_->isConnected()) {
        return;
    }

    auto accounts = account_mgr_->activeAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        rpc_client_->logoutAccount(accounts.at(i));
    }
}

void SeadriveGui::connectDaemon() {
    if (!rpc_client_->tryConnectDaemon()) {
        return;
    }

    connect_daemon_timer_.stop();
    onDaemonStarted();
}

void SeadriveGui::onDaemonRestarted()
{
    qDebug("reviving rpc client when daemon is restarted");
    if (rpc_client_) {
        delete rpc_client_;
    }

    rpc_client_ = new SeafileRpcClient();
    rpc_client_->connectDaemon();

    qDebug("setting account when daemon is restarted");

    auto accounts = account_mgr_->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        rpc_client_->addAccount(accounts.at(i));
    }
}

void SeadriveGui::onDaemonStarted()
{
    // The addAccount() RPC should be invoked after an account being logged in.
    // When launching seadrive-gui, the login event may be raised before the
    // daemon started. For that reason, we queue all account updating events,
    // and begin processing here.
    connect(account_mgr_, SIGNAL(accountMQUpdated()),
            this, SLOT(updateAccountToDaemon()));
    updateAccountToDaemon();

    tray_icon_->start();
    message_poller_->start();
    settings_mgr_->writeProxySettingsToDaemon(settings_mgr_->getProxy());

    QString value;
    if (rpc_client_->seafileGetConfig("client_id", &value) < 0 ||
        value.isEmpty() || value != getUniqueClientId()) {
        rpc_client_->seafileSetConfig("client_id", getUniqueClientId());
        gui->rpcClient()->seafileSetConfig(
            "client_name", gui->settingsManager()->getComputerName());
    }

    RemoteWipeService::instance()->start();
    AccountInfoService::instance()->start();

#if defined(_MSC_VER)
    SeafileExtensionHandler::instance()->start();
    RegElement::installCustomUrlHandler();
#endif

#ifdef HAVE_SPARKLE_SUPPORT
    if (AutoUpdateService::instance()->shouldSupportAutoUpdate()) {
        AutoUpdateService::instance()->start();
    }
#endif // HAVE_SPARKLE_SUPPORT
}

void SeadriveGui::updateAccountToDaemon()
{
    while (!account_mgr_->messages.isEmpty()) {
        auto msg = account_mgr_->messages.dequeue();

        if (msg.type == AccountAdded) {
            rpc_client_->addAccount(msg.account);
        } else if (msg.type == AccountRemoved) {
            rpc_client_->deleteAccount(msg.account);
        }
    }
}

void SeadriveGui::onAboutToQuit()
{
    tray_icon_->hide();

    logoutAccountsFromDaemon();
}

// stop the main event loop and return to the main function
void SeadriveGui::errorAndExit(const QString& error)
{
    qWarning("Exiting with error: %s", toCStr(error));

    if (!started_) {
        warningBox(error);
        ::exit(1);
    }

    if (in_exit_ || QCoreApplication::closingDown()) {
        return;
    }
    in_exit_ = true;

    warningBox(error);

    // stop eventloop before exit and return to the main function
    QCoreApplication::exit(1);
}

void SeadriveGui::restartApp()
{
    if (in_exit_ || QCoreApplication::closingDown()) {
        return;
    }

    in_exit_ = true;

    QStringList args = QApplication::arguments();

    args.removeFirst();

    // append delay argument
    bool found = false;
    Q_FOREACH(const QString& arg, args)
    {
        if (arg == "--delay" || arg == "-D") {
            found = true;
            break;
        }
    }

    if (!found)
        args.push_back("--delay");

    QProcess::startDetached(QApplication::applicationFilePath(), args);
    QCoreApplication::quit();
}

bool SeadriveGui::initLog()
{
    QDir seadrive_dir = seadriveDir();
    if (checkdir_with_mkdir(toCStr(seadrive_dir.absolutePath())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create %1 folder").arg(getBrand()));
        return false;
    }
    if (checkdir_with_mkdir(toCStr(logsDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create %1 logs folder").arg(getBrand()));
        return false;
    }
    if (checkdir_with_mkdir(toCStr(seadriveDataDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create %1 data folder").arg(getBrand()));
        return false;
    }

#if !defined(Q_OS_WIN32)
    if (checkdir_with_mkdir(toCStr(gui->mountDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create seadrive mount folder"));
        return false;
    }
#endif

    if (applet_log_init(toCStr(seadrive_dir.absolutePath())) < 0) {
        errorAndExit(tr("Failed to initialize log: %1").arg(g_strerror(errno)));
        return false;
    }

    // give a change to override DEBUG_LEVEL by environment
    QString debug_level = qgetenv("SEAFILE_CLIENT_DEBUG");
    if (!debug_level.isEmpty() && debug_level != "false" &&
        debug_level != "0") {
        seafile_client_debug_level = DEBUG;
        printf ("debug enabled from env\n");
    } else if (debugEnabledInDebugFlagFile()) {
        seafile_client_debug_level = DEBUG;
        printf ("debug enabled from ~/seafile-client-debug.txt\n");
    }

    if (seafile_client_debug_level == DEBUG)
        qInstallMessageHandler(myLogHandlerDebug);
    else
        qInstallMessageHandler(myLogHandler);

    return true;
}

bool SeadriveGui::loadQss(const QString& path)
{
    QFile file(path);
    if (!QFileInfo(file).exists()) {
        return false;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream input(&file);
    style_ += "\n";
    style_ += input.readAll();
    qApp->setStyleSheet(style_);

    return true;
}

void SeadriveGui::refreshQss()
{
    style_.clear();
    loadQss("qt.css") || loadQss(":/qt.css");

#if defined(Q_OS_WIN32)
    loadQss("qt-win.css") || loadQss(":/qt-win.css");
#elif defined(Q_OS_LINUX)
    loadQss("qt-linux.css") || loadQss(":/qt-linux.css");
#else
    loadQss("qt-mac.css") || loadQss(":/qt-mac.css");
#endif
}

void SeadriveGui::warningBox(const QString& msg, QWidget *parent)
{
    QMessageBox box(parent);
    box.setText(msg);
    box.setWindowTitle(getBrand());
    box.setIcon(QMessageBox::Warning);
    box.addButton(tr("OK"), QMessageBox::YesRole);
    box.exec();

    qWarning("%s", msg.toUtf8().data());
}

void SeadriveGui::messageBox(const QString& msg, QWidget *parent)
{
    QMessageBox box(parent);
    box.setText(msg);
    box.setWindowTitle(getBrand());
    box.setIcon(QMessageBox::Information);
    box.addButton(tr("OK"), QMessageBox::YesRole);
    box.exec();
    qDebug("%s", msg.toUtf8().data());
}

bool SeadriveGui::yesOrNoBox(const QString& msg, QWidget *parent, bool default_val)
{
    QMessageBox box(parent);
    box.setText(msg);
    box.setWindowTitle(getBrand());
    box.setIcon(QMessageBox::Question);
    QPushButton *yes_btn = box.addButton(tr("Yes"), QMessageBox::YesRole);
    QPushButton *no_btn = box.addButton(tr("No"), QMessageBox::NoRole);
    box.setDefaultButton(default_val ? yes_btn: no_btn);
    box.exec();

    return box.clickedButton() == yes_btn;
}

bool SeadriveGui::yesOrCancelBox(const QString& msg, QWidget *parent, bool default_yes)
{
    QMessageBox box(parent);
    box.setText(msg);
    box.setWindowTitle(getBrand());
    box.setIcon(QMessageBox::Question);
    QPushButton *yes_btn = box.addButton(tr("Yes"), QMessageBox::YesRole);
    QPushButton *cancel_btn = box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(default_yes ? yes_btn: cancel_btn);
    box.exec();

    return box.clickedButton() == yes_btn;
}


QMessageBox::StandardButton
SeadriveGui::yesNoCancelBox(const QString& msg, QWidget *parent, QMessageBox::StandardButton default_btn)
{
    QMessageBox box(parent);
    box.setText(msg);
    box.setWindowTitle(getBrand());
    box.setIcon(QMessageBox::Question);
    QPushButton *yes_btn = box.addButton(tr("Yes"), QMessageBox::YesRole);
    QPushButton *no_btn = box.addButton(tr("No"), QMessageBox::NoRole);
    box.addButton(tr("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(default_btn);
    box.exec();

    QAbstractButton *btn = box.clickedButton();
    if (btn == yes_btn) {
        return QMessageBox::Yes;
    } else if (btn == no_btn) {
        return QMessageBox::No;
    }

    return QMessageBox::Cancel;
}

bool SeadriveGui::detailedYesOrNoBox(const QString& msg, const QString& detailed_text, QWidget *parent, bool default_val)
{
    QMessageBox msgBox(QMessageBox::Question,
                       getBrand(),
                       msg,
                       QMessageBox::Yes | QMessageBox::No,
                       parent);
    msgBox.setDetailedText(detailed_text);
    msgBox.setButtonText(QMessageBox::Yes, tr("Yes"));
    msgBox.setButtonText(QMessageBox::No, tr("No"));
    // Turns out the layout box in the QMessageBox is a grid
    // You can force the resize using a spacer this way:
    QSpacerItem* horizontalSpacer = new QSpacerItem(400, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
    QGridLayout* layout = (QGridLayout*)msgBox.layout();
    layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());
    msgBox.setDefaultButton(default_val ? QMessageBox::Yes : QMessageBox::No);
    return msgBox.exec() == QMessageBox::Yes;
}

QVariant SeadriveGui::readPreconfigureEntry(const QString& key, const QVariant& default_value)
{
#ifdef Q_OS_WIN32
    QVariant v = RegElement::getPreconfigureValue(key);
    if (!v.isNull()) {
        return v;
    }
#endif
    QString configure_file = QDir::home().filePath(kSeafileConfigureFileName);
    if (!QFileInfo(configure_file).exists())
        return default_value;
    QSettings setting(configure_file, QSettings::IniFormat);
    setting.beginGroup(kSeafilePreconfigureGroupName);
    QVariant value = setting.value(key, default_value);
    setting.endGroup();
    return value;
}

QString SeadriveGui::readPreconfigureExpandedString(const QString& key, const QString& default_value)
{
    QVariant retval = readPreconfigureEntry(key, default_value);
    if (retval.isNull() || retval.type() != QVariant::String)
        return QString();
    return expandVars(retval.toString());
}

QString SeadriveGui::seadriveDir() const
{
    return kSeadriveDirName;
}

QString SeadriveGui::seadriveDataDir() const
{
    return QDir(seadriveDir()).filePath("data");
}

QString SeadriveGui::logsDir() const
{
    return QDir(seadriveDir()).filePath("logs");
}

QString SeadriveGui::mountDir() const
{
#if defined(Q_OS_WIN32)
    QString sync_root_name = gui->accountManager()->getSyncRootName();
    if (sync_root_name.isEmpty()) {
        qWarning("get sync root name is empty.");
    }

    QString sync_root = ::pathJoin(seadriveRoot(), sync_root_name);
    return sync_root;
#else
    return QDir::home().absoluteFilePath(getBrand());
#endif
}

#if defined(_MSC_VER)
QString SeadriveGui::seadriveRoot() const
{
    return seadrive_root_;
}
#endif

QString SeadriveGui::getUniqueClientId()
{
    // Id file path is `~/.seadrive/id`
    QFile id_file(QDir(seadriveDir()).absoluteFilePath("id"));
    if (!id_file.exists()) {
        srand(time(NULL));
        QString id;
        while (id.length() < 40) {
            int r = rand() % 0xff;
            id += QString("%1").arg(r, 0, 16);
        }
        id = id.mid(0, 40);

        if (!id_file.open(QIODevice::WriteOnly)) {
            errorAndExit(tr("failed to save client id"));
            return "";
        }

        id_file.write(id.toUtf8().data());
        return id;
    }

    if (!id_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        errorAndExit(tr("failed to access %1").arg(id_file.fileName()));
        return "";
    }

    QTextStream input(&id_file);
#if (QT_VERSION >= QT_VERSION_CHECK(6,0,0))
    input.setEncoding(QStringConverter::Utf8);
#else
    input.setCodec("UTF-8");
#endif

    if (input.atEnd()) {
        errorAndExit(tr("incorrect client id"));
        return "";
    }

    QString id = input.readLine().trimmed();
    if (id.length() != 40) {
        errorAndExit(tr("failed to read %1").arg(id_file.fileName()));
        return "";
    }

    return id;
}
