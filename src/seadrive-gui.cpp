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

#include <errno.h>
#include <glib.h>

#include "utils/utils.h"
#include "utils/file-utils.h"
#include "utils/log.h"
#include "ui/tray-icon.h"
#include "ui/login-dialog.h"
#include "ui/settings-dialog.h"
#include "ui/about-dialog.h"
#include "daemon-mgr.h"
#include "rpc/rpc-client.h"
#include "account-mgr.h"
#include "settings-mgr.h"
#include "message-poller.h"
#include "remote-wipe-service.h"
#include "account-info-service.h"
#ifdef HAVE_FINDER_SYNC_SUPPORT
#include "finder-sync/finder-sync-listener.h"
#endif

#if defined(Q_OS_WIN32)
#include "utils/registry.h"
#include "utils/utils-win.h"
#include "ext-handler.h"
#include "ui/disk-letter-dialog.h"
#endif

#ifdef HAVE_SPARKLE_SUPPORT
#include "auto-update-service.h"
#endif

#if defined(Q_OS_MAC)
#include "utils/utils-mac.h"
#include "osx-helperutils/osx-helperutils.h"
#endif

#include "seadrive-gui.h"

namespace {

#if defined(Q_OS_WIN32)
    const char *kSeadriveDirName = "seadrive";
#else
    const char *kSeadriveDirName = ".seadrive";
#endif

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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
        g_info("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
#endif
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
        g_info("%s\n", localMsg.constData());
        break;
#endif
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
        g_info("%s (%s:%u)\n", localMsg.constData(), context.file, context.line);
        break;
#endif
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
#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
        g_info("%s\n", localMsg.constData());
        break;
#endif
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


#ifdef Q_OS_MAC
void writeCABundleForCurl()
{
    QString current_cache_dir;
    if (!gui->settingsManager()->getCacheDir(&current_cache_dir)){
        current_cache_dir = QDir(gui->seadriveDataDir()).absolutePath();

    }
    QString ca_bundle_path = pathJoin(current_cache_dir, "ca-bundle.pem");
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
const char *const kSeafileConfigureFileName = "seafile.ini";
const char *const kSeafileConfigurePath = "SOFTWARE\\Seafile";
const int kIntervalBeforeShowInitVirtualDialog = 3000;
#else
const char *const kSeafileConfigureFileName = ".seafilerc";
#endif
const char *const kSeafilePreconfigureGroupName = "preconfigure";

} // namespace


SeadriveGui *gui;

SeadriveGui::SeadriveGui()
    : started_(false),
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
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onAboutToQuit()));
}

SeadriveGui::~SeadriveGui()
{
    // Must unmount before rpc client is destroyed.
    daemon_mgr_->doUnmount();
#ifdef HAVE_SPARKLE_SUPPORT
    AutoUpdateService::instance()->stop();
#endif

    delete tray_icon_;
    delete rpc_client_;
    delete daemon_mgr_;
    delete account_mgr_;
    delete message_poller_;
}

void SeadriveGui::start()
{
    started_ = true;

    if (!initLog()) {
        return;
    }

#if defined(Q_OS_MAC)
#ifdef XCODE_APP
    bool require_user_approval = false;
    if (!installHelperAndKext(&require_user_approval)) {
        if (require_user_approval) {
            qWarning("the kext requires user approval");

            messageBox(
                tr("You need to approve %1 kernel extension manually in the "
                   "system preferences. Click OK to open the system "
                   "preferences dialog. Please re-launch %1 after that.").arg(getBrand()));

            // Open the system preferences for the user and exit.
            QStringList args;
            args << "x-apple.systempreferences:com.apple.preference.security?General";
            QProcess::execute("open", args);
            QCoreApplication::exit(1);
        } else {
            errorAndExit(tr("Failed to initialize: failed to install kernel driver"));
        }
        return;
    }
#endif
#endif

    qDebug("client id is %s", toCStr(getUniqueClientId()));

    account_mgr_->start();
    
    // auto update rpc server start
    SeaDriveRpcServer::instance()->start();

    refreshQss();

    qWarning("seadrive gui started");

#if defined(Q_OS_MAC)
    writeCABundleForCurl();
#endif

    // Load system proxy information. This must be done before we start seadrive
    // daemon.
    settings_mgr_->writeSystemProxyInfo(
        account_mgr_->currentAccount().serverUrl,
        QDir(seadriveDataDir()).filePath("system-proxy.txt"));

#if defined(Q_OS_WIN32)
    QString disk_letter;
    if (settings_mgr_->getDiskLetter(&disk_letter)) {
        disk_letter_ = disk_letter;
    } else {
        qWarning("disk letter not set, asking the user for it");
        DiskLetterDialog dialog;
        if (dialog.exec() != QDialog::Accepted) {
            errorAndExit(tr("Faild to choose a disk letter"));
            return;
        }
        disk_letter_ = dialog.diskLetter();
        settings_mgr_->setDiskLetter(disk_letter_);
    }
    qWarning("Using disk letter %s", toCStr(disk_letter_));
#endif

    connect(daemon_mgr_, SIGNAL(daemonStarted()),
            this, SLOT(onDaemonStarted()));
    connect(daemon_mgr_, SIGNAL(daemonRestarted()),
            this, SLOT(onDaemonRestarted()));
    daemon_mgr_->startSeadriveDaemon();

#if defined(Q_OS_WIN32)
    QString program = "csmcmd.exe";
    QStringList arguments;
    // Exclude the file-cache folder from being indexed by windows search.
    QString cache_path = QDir::toNativeSeparators(seadriveDataDir() + "/file-cache/*");
    arguments << "/add_rule" << cache_path << "/default";

    QProcess::execute(program, arguments);
#endif
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
    const Account &account = account_mgr_->currentAccount();
    if (account.isValid()) {
        rpc_client_->switchAccount(account);
    }
}

void SeadriveGui::onDaemonStarted()
{
    rpc_client_->connectDaemon();
    //
    // load proxy settings (important)
    //
    settings_mgr_->loadSettings();

    if (first_use_ || account_mgr_->accounts().size() == 0) {
        do {
            QString username = readPreconfigureExpandedString(kPreconfigureUsername);
            QString token = readPreconfigureExpandedString(kPreconfigureUserToken);
            QString url = readPreconfigureExpandedString(kPreconfigureServerAddr);
            QString computer_name = readPreconfigureExpandedString(kPreconfigureComputerName, settingsManager()->getComputerName());
            if (!computer_name.isEmpty())
                settingsManager()->setComputerName(computer_name);
            if (!username.isEmpty() && !token.isEmpty() && !url.isEmpty()) {
                Account account(url, username, token);
                if (account_mgr_->saveAccount(account) < 0) {
                    errorAndExit(tr("failed to add default account"));
                    return;
                }
                break;
            }

            if (readPreconfigureEntry(kHideConfigurationWizard).toInt())
                break;

            LoginDialog login_dialog;
            login_dialog.exec();
        } while (0);
    } else {
        if (!account_mgr_->accounts().empty()) {
            const Account &account = account_mgr_->accounts()[0];
            account_mgr_->validateAndUseAccount(account);
        }
    }

    tray_icon_->start();
    tray_icon_->setState(SeafileTrayIcon::STATE_DAEMON_UP);
    message_poller_->start();

    // Set the device id to the daemon so it can use it when generating commits.
    // The "client_name" is not set here, but updated each time we call
    // switch_account rpc.
    QString value;
    if (rpc_client_->seafileGetConfig("client_id", &value) < 0 ||
        value.isEmpty() || value != getUniqueClientId()) {
        rpc_client_->seafileSetConfig("client_id", getUniqueClientId());
    }

    RemoteWipeService::instance()->start();
    AccountInfoService::instance()->start();

#if defined(Q_OS_WIN32)
    SeafileExtensionHandler::instance()->start();
#endif

#ifdef HAVE_SPARKLE_SUPPORT
    if (AutoUpdateService::instance()->shouldSupportAutoUpdate()) {
        AutoUpdateService::instance()->start();
    }
#endif // HAVE_SPARKLE_SUPPORT


#ifdef HAVE_FINDER_SYNC_SUPPORT
    finderSyncListenerStart();
#endif
}

void SeadriveGui::onAboutToQuit()
{
    tray_icon_->hide();
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

    daemon_mgr_->doUnmount();

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
        errorAndExit(tr("Failed to initialize: failed to create seadrive folder"));
        return false;
    }
    if (checkdir_with_mkdir(toCStr(logsDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create seadrive logs folder"));
        return false;
    }
    if (checkdir_with_mkdir(toCStr(seadriveDataDir())) < 0) {
        errorAndExit(tr("Failed to initialize: failed to create seadrive data folder"));
        return false;
    }

    // On linux we must unmount the mount point dir before trying to create it,
    // otherwise checkdir_with_mkdir would think it doesn't exist and try to
    // create it, but the creation operation would fail.
#if defined(Q_OS_LINUX)
        QStringList umount_arguments;
        umount_arguments << "-u" << mountDir();
        QProcess::execute("fusermount", umount_arguments);
#endif

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
        debug_level != "0")
        seafile_client_debug_level = DEBUG;

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
    return QDir::home().absoluteFilePath(kSeadriveDirName);
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
    return disk_letter_;
#else
    return QDir::home().absoluteFilePath(getBrand());
#endif
}

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
    input.setCodec("UTF-8");

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
