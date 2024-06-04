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
#include <sqlite3.h>

#include "utils/utils.h"
#include "utils/file-utils.h"
#include "utils/log.h"
#include "ui/tray-icon.h"
#include "ui/login-dialog.h"
#include "win-sso/auto-logon-dialog.h"
#include "ui/settings-dialog.h"
#include "ui/about-dialog.h"
#include "ui/init-sync-dialog.h"
#include "daemon-mgr.h"
#include "rpc/rpc-client.h"
#include "account-mgr.h"
#include "settings-mgr.h"
#include "message-poller.h"
#include "remote-wipe-service.h"
#include "account-info-service.h"
#include "file-provider-mgr.h"
#if defined(Q_OS_WIN32)
#include "thumbnail-service.h"
#endif

#if defined(Q_OS_WIN32)
#include "utils/registry.h"
#include "utils/utils-win.h"
#include "ext-handler.h"
#include "ui/seadrive-root-dialog.h"
#endif

#if defined(Q_OS_MAC)
#include "utils/utils-mac.h"
#endif

#include "seadrive-gui.h"

namespace {

#ifdef Q_OS_WIN32
    const char *kPreconfigureCacheDirectory = "PreconfigureCacheDirectory";
#endif

const int kConnectDaemonIntervalMsec = 1000;

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
    QString dir = seadriveDir();
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
      first_use_(false),
      tray_icon_started_(false)
{
    startup_time_ = QDateTime::currentMSecsSinceEpoch();
    tray_icon_ = new SeafileTrayIcon(this);
    daemon_mgr_ = new DaemonManager();
    account_mgr_ = new AccountManager();
    settings_mgr_ = new SettingsManager();
    settings_dlg_ = new SettingsDialog();
    about_dlg_ = new AboutDialog();
    init_sync_dlg_ = new InitSyncDialog();

#if defined(Q_OS_MAC)
    file_provider_mgr_ = new FileProviderManager();
#endif

    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));
}

SeadriveGui::~SeadriveGui()
{
    delete tray_icon_;
    delete daemon_mgr_;
    delete account_mgr_;

    QMapIterator<QString, SeafileRpcClient *> it1(rpc_clients_);
    while (it1.hasNext()) {
        it1.next();
        auto rpc_client = it1.value();
        delete rpc_client;
    }

    QMapIterator<QString, MessagePoller *> it2(message_pollers_);
    while (it2.hasNext()) {
        it2.next();
        auto message_poller = it2.value();
        delete message_poller;
    }

#if defined(Q_OS_MAC)
    delete file_provider_mgr_;
#endif

}

#ifdef Q_OS_MAC
bool loadConfigCB(sqlite3_stmt *stmt, void *data)
{
    SettingsManager *mgr = static_cast<SettingsManager* >(data);
    const char *key = (const char *)sqlite3_column_text (stmt, 0);
    const char *value = (const char *)sqlite3_column_text (stmt, 1);
    if (strcmp(key, "notify_sync") == 0) {
        if (strcmp(value, "on") == 0) {
            mgr->setNotify(true);
        } else {
            mgr->setNotify(false);
        }
    } else if (strcmp(key, "download_limit") == 0) {
        int rate = atoi(value);
        mgr->setMaxDownloadRatio(rate >> 10);
    } else if (strcmp(key, "upload_limit") == 0) {
        int rate = atoi(value);
        mgr->setMaxUploadRatio(rate >> 10);
    } else if (strcmp(key, "clean_cache_interval") == 0) {
        int interval = atoi(value);
        mgr->setCacheCleanIntervalMinutes(interval);
    } else if (strcmp(key, "cache_size_limit") == 0) {
        int limit = atoi(value);
        mgr->setCacheSizeLimitGB(limit);
    } else if (strcmp(key, "sync_extra_temp_file") == 0) {
        if (strcmp(value, "true") == 0) {
            mgr->setSyncExtraTempFile(true);
        } else {
            mgr->setSyncExtraTempFile(false);
        }
    } else if (strcmp(key, "disable_verify_certificate") == 0) {
        if (strcmp(value, "true") == 0) {
            mgr->setHttpSyncCertVerifyDisabled(true);
        } else {
            mgr->setHttpSyncCertVerifyDisabled(false);
        }
    } else if (strcmp(key, "delete_confirm_threshold") == 0) {
        int threshold = atoi(value);
        mgr->setDeleteConfirmThreshold(threshold);
    } else if (strcmp(key, "hide_windows_incompatible_path_notification") == 0) {
        if (strcmp(value, "true") == 0) {
            mgr->setHideWindowsIncompatibilityPathMsg(true);
        } else {
            mgr->setHideWindowsIncompatibilityPathMsg(false);
        }
    }
    return true;
}

void SeadriveGui::migrateOldConfig(const QString& dataDir)
{
    const char *errmsg;
    struct sqlite3 *db = NULL;

    QString db_path = QDir(dataDir).filePath("Config.db");
    if (sqlite3_open (toCStr(db_path), &db)) {
        errmsg = sqlite3_errmsg (db);
        qCritical("failed to open config database %s: %s",
                toCStr(db_path), errmsg ? errmsg : "no error given");

        return;
    }

    const char *sql = "SELECT key, value FROM Config";
    sqlite_foreach_selected_row (db, sql, loadConfigCB, settings_mgr_);

    sqlite3_close(db);
}

void SeadriveGui::migrateOldData()
{
    QString data_dir = QDir(seadriveDir()).filePath("data");
    if (!QDir(data_dir).exists())
        return;

    qWarning("start migrating old data to new version");
    migrateOldConfig(data_dir);

    auto accounts = account_mgr_->allAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        auto account = accounts.at(i); 
        file_provider_mgr_->disconnect(account);
        QString dst_path = QDir(seadriveDir()).filePath(account.domainID());
        if (!copyDirRecursively(data_dir, dst_path)) {
            errorAndExit(tr("Faild to migrate old data"));
            return;
        }
    }
    if (!QDir(data_dir).removeRecursively()) {
        qWarning("failed to remove data dir: %s\n", strerror(errno));
    }

    for (int i = 0; i < accounts.size(); i++) {
        auto account = accounts.at(i); 
        file_provider_mgr_->connect(account);
    }
    qWarning("finish migrating old data to new version");
}
#endif

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

#ifdef Q_OS_MAC
    migrateOldData();
#endif

    // Load system proxy information. This must be done before we start seadrive
    // daemon.
    QUrl url;
    if (!account_mgr_->allAccounts().empty()) {
        url = account_mgr_->allAccounts().front().serverUrl;
    }
#if defined(Q_OS_MAC)
    settings_mgr_->writeSystemProxyInfo(
        url, QDir(seadriveDir()).filePath("system-proxy.txt"));
#else
    settings_mgr_->writeSystemProxyInfo(
        url, QDir(seadriveDataDir()).filePath("system-proxy.txt"));
#endif

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

    RemoteWipeService::instance()->start();
    AccountInfoService::instance()->start();
#endif
}

void SeadriveGui::loginAccounts()
{
    tray_icon_->show();

    if (first_use_ || account_mgr_->allAccounts().size() == 0) {
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
                // A bug that changes default button styles is fixed here by
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

void SeadriveGui::onDaemonStarted()
{
#ifndef Q_OS_MAC
    SeafileRpcClient *rpc_client = rpcClient(EMPTY_DOMAIN_ID);
    if (!rpc_client) {
        rpc_client = new SeafileRpcClient(EMPTY_DOMAIN_ID);
        rpc_clients_.insert(EMPTY_DOMAIN_ID, rpc_client);
        MessagePoller *message_poller = new MessagePoller();
        message_poller->setRpcClient(rpc_client);
        message_poller->start();
        message_pollers_.insert(EMPTY_DOMAIN_ID, message_poller);
    }
#if defined(Q_OS_WIN32)
    rpc_client->connectDaemon();
#endif

    auto accounts = account_mgr_->activeAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        auto account = accounts.at(i);
        rpc_client->addAccount(account);
    }

    tray_icon_->start();
    settings_mgr_->writeSettingsToDaemon();

    QString value;
    if (rpc_client->seafileGetConfig("client_id", &value) < 0 ||
        value.isEmpty() || value != getUniqueClientId()) {
        rpc_client->seafileSetConfig("client_id", getUniqueClientId());
        rpc_client->seafileSetConfig(
            "client_name", gui->settingsManager()->getComputerName());
    }

    RemoteWipeService::instance()->start();
    AccountInfoService::instance()->start();

#if defined(_MSC_VER)
    SeafileExtensionHandler::instance()->start();
    RegElement::installCustomUrlHandler();
#endif

#if defined(Q_OS_WIN32)
    ThumbnailService::instance()->start();
#endif
#endif
}

void SeadriveGui::onDaemonRestarted()
{
#ifndef Q_OS_MAC
    qDebug("reviving rpc client when daemon is restarted");
    SeafileRpcClient *rpc_client = rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_clients_.remove(EMPTY_DOMAIN_ID);
        delete rpc_client;
    }
    rpc_client = new SeafileRpcClient(EMPTY_DOMAIN_ID);
    rpc_clients_.insert(EMPTY_DOMAIN_ID, rpc_client);
    rpc_client->connectDaemon();

    qDebug("setting account when daemon is restarted");

    auto accounts = account_mgr_->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        rpc_client->addAccount(accounts.at(i));
    }
    MessagePoller *message_poller = messagePoller(EMPTY_DOMAIN_ID);
    if (message_poller) {
        message_poller->setRpcClient (rpc_client);
    } else {
        message_poller = new MessagePoller();
        message_poller->setRpcClient(rpc_client);
        message_poller->start();
        message_pollers_.insert(EMPTY_DOMAIN_ID, message_poller);
    }
#endif
}

void SeadriveGui::onDaemonRestarted(const QString& domain_id)
{
#ifdef Q_OS_MAC
    auto account = account_mgr_->getAccountByDomainID(domain_id);
    account_mgr_->setAccountAdded(account, false);
#endif
}

// Traverse all accounts in the account manager every second, create an rpc client and connect to the domain for each account.
void SeadriveGui::connectDaemon()
{
#ifdef Q_OS_MAC
    bool success = false;
    auto accounts = account_mgr_->activeAccounts();

    for (int i = 0; i < accounts.size(); i++) {
        auto account = accounts.at(i);
        // If account was added to daemon before, the rpc client should have been created
        // and the connection is managed by itself.
        if (account.added) {
            continue;
        }
        QString domain_id = account.domainID();
        SeafileRpcClient *rpc_client = rpcClient(domain_id); 
        if (!rpc_client) {
            rpc_client = new SeafileRpcClient(domain_id);
            rpc_clients_.insert(domain_id, rpc_client);
            MessagePoller *message_poller = new MessagePoller();
            message_poller->setRpcClient(rpc_client);
            message_poller->start();
            message_pollers_.insert(domain_id, message_poller);
            connect(rpc_client, SIGNAL(daemonRestarted(const QString&)),
                    this, SLOT(onDaemonRestarted(const QString&)));
        }

        if (!rpc_client->isConnected()) {
            account_mgr_->setAccountAdded(account, false);
            rpc_client->tryConnectDaemon();
            if (!rpc_client->isConnected() && !account.notified_start_extension) {
                if (account.connect_daemon_retry > 5) {
                    account_mgr_->setAccountNotifiedStartExtension(account, true);
                    if (file_provider_mgr_->hasEnabledDomains())
                        messageBox(tr("To start %1 extension for account %2, you need to click the %3 entry in Finder").arg(getBrand()).arg(account.username).arg(getBrand()));
                }
                account_mgr_->addAccountConnectDaemonRetry(account);
            }
        }

        if (rpc_client->isConnected()) {
            account_mgr_->setAccountAdded(account, true);
            rpc_client->addAccount(account);
            settings_mgr_->writeSettingsToDaemon();

            QString value;
            if (rpc_client->seafileGetConfig("client_id", &value) < 0 ||
                value.isEmpty() || value != getUniqueClientId()) {
                rpc_client->seafileSetConfig("client_id", getUniqueClientId());
                rpc_client->seafileSetConfig(
                    "client_name", gui->settingsManager()->getComputerName());
            }
            // The init sync dlg only launches when there is a new logged in account.
            if (init_sync_dlg_->hasNewLogin()) {
                init_sync_dlg_->launch(account.domainID());
            }
            success = true;
        }
    }

    QMapIterator<QString, SeafileRpcClient *> it(rpc_clients_);
    while (it.hasNext()) {
        it.next();
        auto rpc_client = it.value();
        auto domain_id = it.key();
        auto account = account_mgr_->getAccountByDomainID(domain_id);
        if (!account.isValid()) {
            // account has beed deleted, remove account from domain.
            rpc_client->deleteDomainAccount(domain_id);
            MessagePoller *message_poller = messagePoller(domain_id);
            if (message_poller) {
                message_pollers_.remove(domain_id);
                message_poller->stop();
                delete message_poller;
            }
            rpc_clients_.remove(domain_id);
            delete rpc_client;
            init_sync_dlg_->clearPoller(domain_id);
        }
    }

    if (success && !tray_icon_started_) {
        tray_icon_started_ = true;
        tray_icon_->start();
    }
#endif
}

#ifdef Q_OS_MAC
void SeadriveGui::logoutAccountsFromDaemon(const Account& account)
{
    SeafileRpcClient *rpc_client = rpcClient(account.domainID());
    if (!rpc_client) {
        return;
    }

    rpc_client->logoutAccount(account);
}
#endif

void SeadriveGui::onAboutToQuit()
{
    tray_icon_->hide();

#if defined(Q_OS_MAC)
    auto accounts = account_mgr_->activeAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        logoutAccountsFromDaemon(accounts.at(i));
    }
#endif
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
    if (checkdir_with_mkdir(toCStr(seadriveLogDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create %1 logs folder").arg(getBrand()));
        return false;
    }
#if !defined(Q_OS_MAC)
    if (checkdir_with_mkdir(toCStr(seadriveDataDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create %1 data folder").arg(getBrand()));
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
    box.setFocusPolicy(Qt::ClickFocus);
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
    box.setFocusPolicy(Qt::ClickFocus);
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
    box.setFocusPolicy(Qt::ClickFocus);
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
    box.setFocusPolicy(Qt::ClickFocus);
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
    box.setFocusPolicy(Qt::ClickFocus);
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
    msgBox.setFocusPolicy(Qt::ClickFocus);
    return msgBox.exec() == QMessageBox::Yes;
}

bool SeadriveGui::deletingConfirmationBox(const QString& text, const QString& info)
{
    QMessageBox box(nullptr);

    box.setText(text);
    box.setInformativeText(info);
    box.setIcon(QMessageBox::Question);

    // Disable the close button
    box.setWindowFlags((box.windowFlags() & ~Qt::WindowCloseButtonHint) | Qt::CustomizeWindowHint);

    auto yesButton = box.addButton(tr("Yes"), QMessageBox::YesRole);
    auto noButton = box.addButton(tr("No"), QMessageBox::NoRole);
    auto settingsButton = box.addButton(tr("Settings"), QMessageBox::NoRole);
    box.setDefaultButton(noButton);

    box.setFocusPolicy(Qt::ClickFocus);
    box.exec();

    if (box.clickedButton() == yesButton) {
        return true;
    } else if (box.clickedButton() == noButton) {
        return false;
    } else if (box.clickedButton() == settingsButton) {
        settings_dlg_->setCurrentTab(1);

        settings_dlg_->show();
        settings_dlg_->raise();
        settings_dlg_->activateWindow();

        return false;
    }

    return false;
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

QString SeadriveGui::readPreconfigureExpandedString(const QString& key, const QVariant& default_value)
{
    QVariant retval = readPreconfigureEntry(key, default_value);
    if (retval.isNull() || retval.type() != QVariant::String)
        return QString();
    return expandVars(retval.toString());
}

#if defined(Q_OS_WIN32)
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

SeafileRpcClient *SeadriveGui::rpcClient(const QString& domain_id)
{
    SeafileRpcClient *rpc_client;
    if (rpc_clients_.contains(domain_id)) {
        return rpc_clients_.value(domain_id);
    }
    return NULL;
}

MessagePoller *SeadriveGui::messagePoller(const QString& domain_id)
{
    if (message_pollers_.contains(domain_id)) {
        return message_pollers_.value(domain_id);
    }
    return NULL;
}
