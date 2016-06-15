#include <QHostInfo>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>

#include "utils/utils.h"
#include "utils/utils-mac.h"
#include "seadrive-gui.h"
#include "ui/tray-icon.h"
// #include "rpc/rpc-client.h"
#include "utils/utils.h"
#include "network-mgr.h"
// #include "ui/main-window.h"
// #include "account-mgr.h"

#if defined(Q_OS_WIN32)
#include "utils/registry.h"
#endif

#ifdef HAVE_FINDER_SYNC_SUPPORT
#include "finder-sync/finder-sync.h"
#endif

#include "settings-mgr.h"

namespace
{
const char *kHideMainWindowWhenStarted = "hideMainWindowWhenStarted";
const char *kHideDockIcon = "hideDockIcon";
const char *kCheckLatestVersion = "checkLatestVersion";
const char *kEnableSyncingWithExistingFolder = "syncingWithExistingFolder";
const char *kBehaviorGroup = "Behavior";

// const char *kDefaultLibraryAlreadySetup = "defaultLibraryAlreadySetup";
// const char *kStatusGroup = "Status";

const char *kSettingsGroup = "Settings";
const char *kComputerName = "computerName";
#ifdef HAVE_FINDER_SYNC_SUPPORT
const char *kFinderSync = "finderSync";
#endif // HAVE_FINDER_SYNC_SUPPORT
#ifdef HAVE_SHIBBOLETH_SUPPORT
const char *kLastShibUrl = "lastShiburl";
#endif // HAVE_SHIBBOLETH_SUPPORT

#ifdef Q_OS_WIN32
QString softwareSeafile()
{
    return QString("SOFTWARE\\%1").arg(getBrand());
}
#endif

} // namespace


SettingsManager::SettingsManager()
    : auto_sync_(true),
      bubbleNotifycation_(true),
      autoStart_(false),
      transferEncrypted_(true),
      allow_repo_not_found_on_server_(false),
      sync_extra_temp_file_(false),
      maxDownloadRatio_(0),
      maxUploadRatio_(0),
      verify_http_sync_cert_disabled_(false)
{
}

void SettingsManager::loadSettings()
{
    QString str;
    // int value;

    // if (gui->rpcClient()->seafileGetConfig("notify_sync", &str) >= 0)
    //     bubbleNotifycation_ = (str == "off") ? false : true;

    // if (gui->rpcClient()->ccnetGetConfig("encrypt_channel", &str) >= 0)
    //     transferEncrypted_ = (str == "off") ? false : true;

    // if (gui->rpcClient()->seafileGetConfigInt("download_limit",
    //                                                  &value) >= 0)
    //     maxDownloadRatio_ = value >> 10;

    // if (gui->rpcClient()->seafileGetConfigInt("upload_limit", &value) >=
    //     0)
    //     maxUploadRatio_ = value >> 10;

    // if (gui->rpcClient()->seafileGetConfig("sync_extra_temp_file",
    //                                               &str) >= 0)
    //     sync_extra_temp_file_ = (str == "true") ? true : false;

    // if (gui->rpcClient()->seafileGetConfig(
    //         "allow_repo_not_found_on_server", &str) >= 0)
    //     allow_repo_not_found_on_server_ = (str == "true") ? true : false;

    // if (gui->rpcClient()->seafileGetConfig("disable_verify_certificate",
    //                                               &str) >= 0)
    //     verify_http_sync_cert_disabled_ = (str == "true") ? true : false;

    autoStart_ = get_seafile_auto_start();

#ifdef HAVE_FINDER_SYNC_SUPPORT
    // try to do a reinstall, or we may use findersync somewhere else
    // this action won't stop findersync if running already
    FinderSyncExtensionHelper::reinstall();

    // try to sync finder sync extension settings with the actual settings
    // i.e. enabling the finder sync if the setting is true
    setFinderSyncExtension(getFinderSyncExtension());
#endif // HAVE_FINDER_SYNC_SUPPORT


#ifdef Q_OS_WIN32
    RegElement reg(HKEY_CURRENT_USER, softwareSeafile(), "ShellExtDisabled",
                   "");
    shell_ext_enabled_ = !reg.exists();
#endif
}


void SettingsManager::setAutoSync(bool auto_sync)
{
    // if (gui->rpcClient()->setAutoSync(auto_sync) < 0) {
    //     // Error
    //     return;
    // }
    auto_sync_ = auto_sync;
    gui->trayIcon()->setState(
        auto_sync ? SeafileTrayIcon::STATE_DAEMON_UP
                  : SeafileTrayIcon::STATE_DAEMON_AUTOSYNC_DISABLED);
    emit autoSyncChanged(auto_sync);
}

void SettingsManager::setNotify(bool notify)
{
    if (bubbleNotifycation_ != notify) {
        // if (gui->rpcClient()->seafileSetConfig(
        //         "notify_sync", notify ? "on" : "off") < 0) {
        //     // Error
        //     return;
        // }
        bubbleNotifycation_ = notify;
    }
}

void SettingsManager::setAutoStart(bool autoStart)
{
    if (autoStart_ != autoStart) {
        if (set_seafile_auto_start(autoStart) >= 0)
            autoStart_ = autoStart;
    }
}

void SettingsManager::setEncryptTransfer(bool encrypted)
{
    if (transferEncrypted_ != encrypted) {
        // if (gui->rpcClient()->ccnetSetConfig(
        //         "encrypt_channel", encrypted ? "on" : "off") < 0) {
        //     // Error
        //     return;
        // }
        transferEncrypted_ = encrypted;
    }
}

void SettingsManager::setMaxDownloadRatio(unsigned int ratio)
{
    if (maxDownloadRatio_ != ratio) {
        // if (gui->rpcClient()->setDownloadRateLimit(ratio << 10) < 0) {
        //     // Error
        //     return;
        // }
        maxDownloadRatio_ = ratio;
    }
}

void SettingsManager::setMaxUploadRatio(unsigned int ratio)
{
    if (maxUploadRatio_ != ratio) {
        // if (gui->rpcClient()->setUploadRateLimit(ratio << 10) < 0) {
        //     // Error
        //     return;
        // }
        maxUploadRatio_ = ratio;
    }
}

bool SettingsManager::hideMainWindowWhenStarted()
{
    QSettings settings;
    bool hide;

    settings.beginGroup(kBehaviorGroup);
    hide = settings.value(kHideMainWindowWhenStarted, false).toBool();
    settings.endGroup();

    return hide;
}

void SettingsManager::setHideMainWindowWhenStarted(bool hide)
{
    QSettings settings;

    settings.beginGroup(kBehaviorGroup);
    settings.setValue(kHideMainWindowWhenStarted, hide);
    settings.endGroup();
}

bool SettingsManager::hideDockIcon()
{
    QSettings settings;
    bool hide;

    settings.beginGroup(kBehaviorGroup);
    hide = settings.value(kHideDockIcon, false).toBool();
    settings.endGroup();
    return hide;
}

void SettingsManager::setHideDockIcon(bool hide)
{
    QSettings settings;

    settings.beginGroup(kBehaviorGroup);
    settings.setValue(kHideDockIcon, hide);
    settings.endGroup();

    set_seafile_dock_icon_style(hide);
#ifdef Q_OS_MAC
    // for UIElement application, the main window might sink
    // under many applications
    // this will force it to stand before all
    // utils::mac::orderFrontRegardless(gui->mainWindow()->winId());
#endif
}

// void SettingsManager::setDefaultLibraryAlreadySetup()
// {
//     QSettings settings;

//     settings.beginGroup(kStatusGroup);
//     settings.setValue(kDefaultLibraryAlreadySetup, true);
//     settings.endGroup();
// }


// bool SettingsManager::defaultLibraryAlreadySetup()
// {
//     QSettings settings;
//     bool done;

//     settings.beginGroup(kStatusGroup);
//     done = settings.value(kDefaultLibraryAlreadySetup, false).toBool();
//     settings.endGroup();

//     return done;
// }

void SettingsManager::removeAllSettings()
{
    QSettings settings;
    settings.clear();

#if defined(Q_OS_WIN32)
    RegElement::removeRegKey(HKEY_CURRENT_USER, "SOFTWARE", getBrand());
#endif
}

void SettingsManager::setCheckLatestVersionEnabled(bool enabled)
{
    QSettings settings;

    settings.beginGroup(kBehaviorGroup);
    settings.setValue(kCheckLatestVersion, enabled);
    settings.endGroup();
}

bool SettingsManager::isCheckLatestVersionEnabled()
{
    QString brand = getBrand();

    if (brand != "Seafile") {
        return false;
    }

    QSettings settings;
    bool enabled;

    settings.beginGroup(kBehaviorGroup);
    enabled = settings.value(kCheckLatestVersion, true).toBool();
    settings.endGroup();

    return enabled;
}

void SettingsManager::setSyncExtraTempFile(bool sync)
{
    if (sync_extra_temp_file_ != sync) {
        // if (gui->rpcClient()->seafileSetConfig(
        //         "sync_extra_temp_file", sync ? "true" : "false") < 0) {
        //     // Error
        //     return;
        // }
        sync_extra_temp_file_ = sync;
    }
}


void SettingsManager::setAllowRepoNotFoundOnServer(bool val)
{
    if (allow_repo_not_found_on_server_ != val) {
        // if (gui->rpcClient()->seafileSetConfig(
        //         "allow_repo_not_found_on_server", val ? "true" : "false") < 0) {
        //     // Error
        //     return;
        // }
        allow_repo_not_found_on_server_ = val;
    }
}

void SettingsManager::setHttpSyncCertVerifyDisabled(bool disabled)
{
    if (verify_http_sync_cert_disabled_ != disabled) {
        // if (gui->rpcClient()->seafileSetConfig(
        //         "disable_verify_certificate", disabled ? "true" : "false") <
        //     0) {
        //     // Error
        //     return;
        // }
        verify_http_sync_cert_disabled_ = disabled;
    }
}

bool SettingsManager::isEnableSyncingWithExistingFolder() const
{
    bool enabled;
    QSettings settings;

    settings.beginGroup(kBehaviorGroup);
    enabled = settings.value(kEnableSyncingWithExistingFolder, false).toBool();
    settings.endGroup();

    return enabled;
}

void SettingsManager::setEnableSyncingWithExistingFolder(bool enabled)
{
    QSettings settings;

    settings.beginGroup(kBehaviorGroup);
    settings.setValue(kEnableSyncingWithExistingFolder, enabled);
    settings.endGroup();
}

QString SettingsManager::getComputerName()
{
    QSettings settings;
    QString name;

    QString default_computer_Name = QHostInfo::localHostName();

    settings.beginGroup(kSettingsGroup);
    name = settings.value(kComputerName, default_computer_Name).toString();
    settings.endGroup();

    return name;
}

void SettingsManager::setComputerName(const QString &computerName)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kComputerName, computerName);
    settings.endGroup();
}

#ifdef HAVE_SHIBBOLETH_SUPPORT
QString SettingsManager::getLastShibUrl()
{
    QSettings settings;
    QString url;

    settings.beginGroup(kSettingsGroup);
    url = settings.value(kLastShibUrl, "").toString();
    settings.endGroup();

    return url;
}

void SettingsManager::setLastShibUrl(const QString &url)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kLastShibUrl, url);
    settings.endGroup();
}
#endif // HAVE_SHIBBOLETH_SUPPORT

#ifdef HAVE_FINDER_SYNC_SUPPORT
bool SettingsManager::getFinderSyncExtension() const
{
    QSettings settings;
    bool enabled;

    settings.beginGroup(kSettingsGroup);
    enabled = settings.value(kFinderSync, true).toBool();
    settings.endGroup();

    return enabled;
}
bool SettingsManager::getFinderSyncExtensionAvailable() const
{
    return FinderSyncExtensionHelper::isInstalled();
}
void SettingsManager::setFinderSyncExtension(bool enabled)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    settings.setValue(kFinderSync, enabled);
    settings.endGroup();

    // if setting operation fails
    if (!getFinderSyncExtensionAvailable()) {
        qWarning("Unable to find FinderSync Extension");
    } else if (enabled != FinderSyncExtensionHelper::isEnabled() &&
               !FinderSyncExtensionHelper::setEnable(enabled)) {
        qWarning("Unable to enable FinderSync Extension");
    }
}
#endif // HAVE_FINDER_SYNC_SUPPORT

#ifdef Q_OS_WIN32
void SettingsManager::setShellExtensionEnabled(bool enabled)
{
    shell_ext_enabled_ = enabled;

    RegElement reg1(HKEY_CURRENT_USER, softwareSeafile(), "", "");
    RegElement reg2(HKEY_CURRENT_USER, softwareSeafile(), "ShellExtDisabled",
                    "1");
    if (enabled) {
        reg2.remove();
    } else {
        reg1.add();
        reg2.add();
    }
}
#endif // Q_OS_WIN32
