#include <QHostInfo>
#include <QSettings>
#include <QThreadPool>
#include <QTimer>

#include "utils/utils.h"
#include "utils/utils-mac.h"
#include "seadrive-gui.h"
#include "ui/tray-icon.h"
#include "rpc/rpc-client.h"
#include "utils/utils.h"
#include "network-mgr.h"
#include "account-mgr.h"

#if defined(Q_OS_WIN32)
#include "utils/registry.h"
#endif

#include "settings-mgr.h"

namespace
{
const char *kCheckLatestVersion = "checkLatestVersion";
const char *kEnableSearch = "enableSearch";
const char *kBehaviorGroup = "Behavior";
const char *kNotifySync = "notifySync";
const char *kMaxDownloadRatio = "maxDownloadRatio";
const char *kMaxUploadRatio = "maxUploadRatio";
const char *kCacheCleanInterval = "cacheCleanInterval";
const char *kCacheSizeLimit = "cacheSizeLimit";
const char *kSyncExtraTempFile = "syncExtraTempFile";
const char *kDisableVerifyCert = "disableVerifyCert";
const char *kDeleteConfirmThreshold = "deleteConfirmThreshold";
const char *kMigrateStatus = "kMigrateStatus";
const char *kLastOpenSyncDialogTimestamp = "lastOpenSyncDialogTimestamp";

#if defined(_MSC_VER)
const char *kSeadriveRoot = "seadriveRoot";
#endif

// const char *kDefaultLibraryAlreadySetup = "defaultLibraryAlreadySetup";
// const char *kStatusGroup = "Status";

const char *kSettingsGroup = "Settings";
const char *kComputerName = "computerName";
const char *kCacheDir = "cacheDir";
const char *kLastShibUrl = "lastShiburl";

const char *kUseProxy = "use_proxy";
const char *kUseSystemProxy = "use_system_proxy";
const char *kProxyType = "proxy_type";
const char *kProxyAddr = "proxy_addr";
const char *kProxyPort = "proxy_port";
const char *kProxyUsername = "proxy_username";
const char *kProxyPassword = "proxy_password";
#if defined(Q_OS_MAC)
const char * kHideWindowsIncompatiblePathNotification = "hide_windows_incompatible_path_notification";
#endif

const int kCheckSystemProxyIntervalMSecs = 5 * 1000;

bool getSystemProxyForUrl(const QUrl &url, QNetworkProxy *proxy)
{
    QNetworkProxyQuery query(url);
    bool use_proxy = true;
    QList<QNetworkProxy> proxies = QNetworkProxyFactory::systemProxyForQuery(query);

    // printf("list of proxies: %d\n", proxies.size());
    // foreach (const QNetworkProxy &proxy, proxies) {
    //     static int i = 0;
    //     printf("[proxy number %d] %d %s:%d %s %s \n", i++, (int)proxy.type(),
    //            proxy.hostName().toUtf8().data(), proxy.port(),
    //            proxy.user().toUtf8().data(),
    //            proxy.password().toUtf8().data());
    // }

    if (proxies.empty()) {
        use_proxy = false;
    } else {
        *proxy = proxies[0];
        if (proxy->type() == QNetworkProxy::NoProxy ||
            proxy->type() == QNetworkProxy::DefaultProxy ||
            proxy->type() == QNetworkProxy::FtpCachingProxy) {
            use_proxy = false;
        }

        if (proxy->hostName().isEmpty() || proxy->port() == 0) {
            use_proxy = false;
        }
    }

    return use_proxy;
}


#ifdef Q_OS_WIN32
QString softwareSeaDrive()
{
    return QString("SOFTWARE\\%1").arg(getBrand());
}
#endif

} // namespace


SettingsManager::SettingsManager()
    : auto_sync_(true),
      bubbleNotifycation_(true),
      autoStart_(false),
      sync_extra_temp_file_(false),
      maxDownloadRatio_(0),
      maxUploadRatio_(0),
      verify_http_sync_cert_disabled_(false),
      current_proxy_(SeafileProxy()),
      cache_clean_limit_minutes_(10),
      cache_size_limit_gb_(10),
      delete_confirm_threshold_(500)
{
    check_system_proxy_timer_ = new QTimer(this);
    connect(check_system_proxy_timer_, SIGNAL(timeout()), this, SLOT(checkSystemProxy()));
}

void SettingsManager::loadSettings()
{
    bubbleNotifycation_ = getNotify();

    maxDownloadRatio_ = geteMaxDownloadRatio();

    maxUploadRatio_ = geteMaxUploadRatio();

    sync_extra_temp_file_ = getSyncExtraTempFile();

    verify_http_sync_cert_disabled_ = getHttpSyncCertVerifyDisabled();

    cache_size_limit_gb_ = qMax(1, getCacheSizeLimitGB());

    cache_clean_limit_minutes_ = qMax(1, getCacheCleanIntervalMinutes());

    int value = getDeleteConfirmThreshold();
    if (value >= 0)
        delete_confirm_threshold_ = value;

    loadProxySettings();
    applyProxySettings();

    autoStart_ = get_seafile_auto_start();

#ifdef Q_OS_WIN32
    RegElement reg(HKEY_CURRENT_USER, softwareSeaDrive(), "ShellExtDisabled",
                   "");
    shell_ext_enabled_ = !reg.exists();
#endif
}

void SettingsManager::loadProxySettings()
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);

    SeafileProxy proxy;
    proxy.type = static_cast<ProxyType>(settings.value(kProxyType, SystemProxy).toInt());
    proxy.host = settings.value(kProxyAddr, "").toString();
    proxy.port = settings.value(kProxyPort, 0).toInt();
    proxy.username = settings.value(kProxyUsername, "").toString();
    proxy.password = settings.value(kProxyPassword, "").toString();

    current_proxy_ = proxy;
}

void SettingsManager::setNotify(bool notify)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kNotifySync, notify);
    settings.endGroup();

    if (bubbleNotifycation_ != notify) {
        bubbleNotifycation_ = notify;
    }
#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->seafileSetConfig(
                    "notify_sync", notify ? "on" : "off");
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->seafileSetConfig(
                "notify_sync", notify ? "on" : "off");
    }
#endif
}

bool SettingsManager::getNotify()
{
    QSettings settings;
    bool notify;

    settings.beginGroup(kSettingsGroup);
    notify = settings.value(kNotifySync, true).toBool();
    settings.endGroup();

    return notify;
}

void SettingsManager::setAutoStart(bool autoStart)
{
    if (autoStart_ != autoStart) {
        if (set_seafile_auto_start(autoStart) >= 0)
            autoStart_ = autoStart;
    }
}

void SettingsManager::setMaxDownloadRatio(unsigned int ratio)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kMaxDownloadRatio, ratio);
    settings.endGroup();

    if (maxDownloadRatio_ != ratio) {
        maxDownloadRatio_ = ratio;
    }
#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->setDownloadRateLimit(ratio << 10);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->setDownloadRateLimit(ratio << 10);
    }
#endif
}

unsigned int SettingsManager::geteMaxDownloadRatio()
{
    QSettings settings;
    unsigned int ratio;

    settings.beginGroup(kSettingsGroup);
    ratio = settings.value(kMaxDownloadRatio, 0).toUInt();
    settings.endGroup();

    return ratio;
}

void SettingsManager::setMaxUploadRatio(unsigned int ratio)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kMaxUploadRatio, ratio);
    settings.endGroup();

    if (maxUploadRatio_ != ratio) {
        maxUploadRatio_ = ratio;
    }

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->setUploadRateLimit(ratio << 10);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->setUploadRateLimit(ratio << 10);
    }
#endif
}

unsigned int SettingsManager::geteMaxUploadRatio()
{
    QSettings settings;
    unsigned int ratio;

    settings.beginGroup(kSettingsGroup);
    ratio = settings.value(kMaxUploadRatio, 0).toUInt();
    settings.endGroup();

    return ratio;
}

void SettingsManager::setCacheCleanIntervalMinutes(int interval)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kCacheCleanInterval, interval);
    settings.endGroup();

    if (cache_clean_limit_minutes_ != interval) {
        cache_clean_limit_minutes_ = interval;
    }

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->setCacheCleanIntervalMinutes(interval);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->setCacheCleanIntervalMinutes(interval);
    }
#endif
}

int SettingsManager::getCacheCleanIntervalMinutes()
{
    QSettings settings;
    int interval;

    settings.beginGroup(kSettingsGroup);
    interval = settings.value(kCacheCleanInterval, 10).toInt();
    settings.endGroup();

    return interval;
}

void SettingsManager::setCacheSizeLimitGB(int limit)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kCacheSizeLimit, limit);
    settings.endGroup();

    if (cache_size_limit_gb_ != limit) {
        cache_size_limit_gb_ = limit;
    }

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->setCacheSizeLimitGB(limit);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->setCacheSizeLimitGB(limit);
    }
#endif
}
int SettingsManager::getCacheSizeLimitGB()
{
    QSettings settings;
    int limit;

    settings.beginGroup(kSettingsGroup);
    limit = settings.value(kCacheSizeLimit, 10).toInt();
    settings.endGroup();

    return limit;
}

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
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kSyncExtraTempFile, sync);
    settings.endGroup();

    if (sync_extra_temp_file_ != sync) {
        sync_extra_temp_file_ = sync;
    }

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->seafileSetConfig(
                "sync_extra_temp_file", sync ? "true" : "false");
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->seafileSetConfig(
            "sync_extra_temp_file", sync ? "true" : "false");
    }
#endif
}

bool SettingsManager::getSyncExtraTempFile()
{
    QSettings settings;
    bool sync;

    settings.beginGroup(kSettingsGroup);
    sync = settings.value(kSyncExtraTempFile, false).toBool();
    settings.endGroup();

    return sync;
}

void SettingsManager::setSearchEnabled(bool enabled)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kEnableSearch, enabled);
    settings.endGroup();
}

bool SettingsManager::getSearchEnabled()
{
    QSettings settings;
    bool enabled;

    settings.beginGroup(kSettingsGroup);
    enabled = settings.value(kEnableSearch, false).toBool();
    settings.endGroup();

    return enabled;
}

void SettingsManager::getProxy(QNetworkProxy *proxy) const
{
    current_proxy_.toQtNetworkProxy(proxy);
    return;
}

void SettingsManager::SeafileProxy::toQtNetworkProxy(QNetworkProxy *proxy) const
{
    if (type == NoProxy) {
        proxy->setType(QNetworkProxy::NoProxy);
        return;
    }
    proxy->setType(type == HttpProxy ? QNetworkProxy::HttpProxy
                                     : QNetworkProxy::Socks5Proxy);
    proxy->setHostName(host);
    proxy->setPort(port);
    if ((type == HttpProxy || type == SocksProxy) && !username.isEmpty() && !password.isEmpty()) {
        proxy->setUser(username);
        proxy->setPassword(password);
    }
}

SettingsManager::SeafileProxy SettingsManager::SeafileProxy::fromQtNetworkProxy(
    const QNetworkProxy &proxy)
{
    SeafileProxy sproxy;
    if (proxy.type() == QNetworkProxy::NoProxy ||
        proxy.type() == QNetworkProxy::DefaultProxy) {
        sproxy.type = NoProxy;
        return sproxy;
    }

    sproxy.host = proxy.hostName();
    sproxy.port = proxy.port();

    if (proxy.type() == QNetworkProxy::HttpProxy) {
        sproxy.type = HttpProxy;
        sproxy.username = proxy.user();
        sproxy.password = proxy.password();
    } else if (proxy.type() == QNetworkProxy::Socks5Proxy) {
        sproxy.type = SocksProxy;
        sproxy.username = proxy.user();
        sproxy.password = proxy.password();
    }

    return sproxy;
}

bool SettingsManager::SeafileProxy::operator==(const SeafileProxy &rhs) const
{
    if (type != rhs.type) {
        return false;
    }
    if (type == NoProxy || type == SystemProxy) {
        return true;
    } else if (type == HttpProxy) {
        return host == rhs.host && port == rhs.port &&
               username == rhs.username && password == rhs.password;
    } else {
        // socks proxy
        return host == rhs.host && port == rhs.port &&
               username == rhs.username && password == rhs.password;
    }
}

void SettingsManager::setProxy(const SeafileProxy &proxy)
{
    if (proxy == current_proxy_) {
        return;
    }
    current_proxy_ = proxy;

    writeProxySettings(proxy);

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            writeProxySettingsToDaemon(accounts.at(i).domainID(), proxy);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        writeProxySettingsToDaemon(EMPTY_DOMAIN_ID, proxy);
    }
#endif
    applyProxySettings();
}

void SettingsManager::applyProxySettings()
{
    if (current_proxy_.type == SystemProxy) {
        QNetworkProxyFactory::setUseSystemConfiguration(true);
        if (!check_system_proxy_timer_->isActive()) {
            check_system_proxy_timer_->start(kCheckSystemProxyIntervalMSecs);
        }
        return;
    } else {
        QNetworkProxyFactory::setUseSystemConfiguration(false);
        if (check_system_proxy_timer_->isActive()) {
            check_system_proxy_timer_->stop();
        }

        if (current_proxy_.type == NoProxy) {
            QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
        }
    }

    QNetworkProxy proxy;
    getProxy(&proxy);
    NetworkManager::instance()->applyProxy(proxy);
}

void SettingsManager::writeProxySettings(const SeafileProxy &proxy)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);

    settings.setValue(kProxyType, static_cast<int>(proxy.type));
    settings.setValue(kProxyAddr, proxy.host);
    settings.setValue(kProxyPort, proxy.port);
    settings.setValue(kProxyUsername, proxy.username);
    settings.setValue(kProxyPassword, proxy.password);

    settings.sync();
}

void SettingsManager::writeProxySettingsToDaemon(const QString& domain_id, const SeafileProxy &proxy)
{
    SeafileRpcClient *rpc = gui->rpcClient(domain_id);
    if (!rpc)
        return;

    if (proxy.type == NoProxy) {
        rpc->seafileSetConfig(kUseProxy, "false");
        return;
    }

    rpc->seafileSetConfig(kUseProxy, "true");
    if (proxy.type == SystemProxy) {
        rpc->seafileSetConfig(kUseSystemProxy, "true");
        return;
    } else {
        rpc->seafileSetConfig(kUseSystemProxy, "false");
    }

    writeProxyDetailsToDaemon(domain_id, proxy);
}

void SettingsManager::writeProxyDetailsToDaemon(const QString& domain_id, const SeafileProxy& proxy)
{
    Q_ASSERT(proxy.type != NoProxy && proxy.type != SystemProxy);
    SeafileRpcClient *rpc = gui->rpcClient(domain_id);
    if (!rpc)
        return;

    QString type = proxy.type == HttpProxy ? "http" : "socks";
    rpc->seafileSetConfig(kProxyType, type);
    rpc->seafileSetConfig(kProxyAddr, proxy.host.toUtf8().data());
    rpc->seafileSetConfigInt(kProxyPort, proxy.port);
    if (type == "http" ||  type == "socks") {
        rpc->seafileSetConfig(kProxyUsername, proxy.username.toUtf8().data());
        rpc->seafileSetConfig(kProxyPassword, proxy.password.toUtf8().data());
    }
}

void SettingsManager::writeSettingsToDaemon()
{
    bool notify = getNotify();
    setNotify(notify);

    bool sync = getSyncExtraTempFile();
    setSyncExtraTempFile(sync);

    unsigned int download_ratio = geteMaxDownloadRatio();
    setMaxDownloadRatio(download_ratio);

    unsigned int upload_ratio = geteMaxUploadRatio();
    setMaxUploadRatio(upload_ratio);

    bool disabled = getHttpSyncCertVerifyDisabled();
    setHttpSyncCertVerifyDisabled(disabled);

#if defined(Q_OS_MAC)
    bool enabled = getHideWindowsIncompatibilityPathMsg();
    setHideWindowsIncompatibilityPathMsg(enabled);
#endif

    int interval = getCacheCleanIntervalMinutes();
    setCacheCleanIntervalMinutes(interval);

    int limit = getCacheSizeLimitGB();
    setCacheSizeLimitGB(limit);

    int value = getDeleteConfirmThreshold();
    setDeleteConfirmThreshold(value);

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        writeProxySettingsToDaemon(accounts.at(i).domainID(), getProxy());
    }
#else
    writeProxySettingsToDaemon(EMPTY_DOMAIN_ID, getProxy());
#endif
}

void SettingsManager::setHttpSyncCertVerifyDisabled(bool disabled)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kDisableVerifyCert, disabled);
    settings.endGroup();

    if (verify_http_sync_cert_disabled_ != disabled) {
        verify_http_sync_cert_disabled_ = disabled;
    }

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->seafileSetConfig(
                "disable_verify_certificate", disabled ? "true" : "false");
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->seafileSetConfig(
            "disable_verify_certificate", disabled ? "true" : "false");
    }
#endif
}

bool SettingsManager::getHttpSyncCertVerifyDisabled()
{
    QSettings settings;
    bool disabled;

    settings.beginGroup(kSettingsGroup);
    disabled = settings.value(kDisableVerifyCert, false).toBool();
    settings.endGroup();

    return disabled;
}

void SettingsManager::setDeleteConfirmThreshold(int value)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kDeleteConfirmThreshold, value);
    settings.endGroup();

    if (delete_confirm_threshold_ != value) {
        delete_confirm_threshold_ = value;
    }

#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->seafileSetConfigInt(
                    "delete_confirm_threshold", value);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->seafileSetConfigInt(
                "delete_confirm_threshold", value);
    }
#endif
}

int SettingsManager::getDeleteConfirmThreshold()
{
    QSettings settings;
    int value;

    settings.beginGroup(kSettingsGroup);
    value = settings.value(kDeleteConfirmThreshold, 500).toInt();
    settings.endGroup();

    return value;
}

#if defined(Q_OS_MAC)
bool SettingsManager::getHideWindowsIncompatibilityPathMsg()
{
    QSettings settings;
    bool enabled;

    settings.beginGroup(kSettingsGroup);
    enabled = settings.value(kHideWindowsIncompatiblePathNotification, false).toBool();
    settings.endGroup();

    return enabled;
}

void SettingsManager::setHideWindowsIncompatibilityPathMsg(bool enabled)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kHideWindowsIncompatiblePathNotification, enabled);
    settings.endGroup();

    QString set_value = enabled == true ? "true" : "false";
#ifdef Q_OS_MAC
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (rpc_client) {
            rpc_client->seafileSetConfig(kHideWindowsIncompatiblePathNotification, set_value);
        }
    }
#else
    SeafileRpcClient *rpc_client = gui->rpcClient(EMPTY_DOMAIN_ID);
    if (rpc_client) {
        rpc_client->seafileSetConfig(kHideWindowsIncompatiblePathNotification, set_value);
    }
#endif
    return;
}
#endif

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

#ifdef Q_OS_WIN32
void SettingsManager::setShellExtensionEnabled(bool enabled)
{
    shell_ext_enabled_ = enabled;

    RegElement reg1(HKEY_CURRENT_USER, softwareSeaDrive(), "", "");
    RegElement reg2(HKEY_CURRENT_USER, softwareSeaDrive(), "ShellExtDisabled",
                    "1");
    if (enabled) {
        reg2.remove();
    } else {
        reg1.add();
        reg2.add();
    }
}

#endif // Q_OS_WIN32

#if defined(_MSC_VER)
bool SettingsManager::getSeadriveRoot(QString *seadrive_root)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    if (!settings.contains(kSeadriveRoot)) {
        return false;
    }

    *seadrive_root = settings.value(kSeadriveRoot, true).toString();
    settings.endGroup();

    return true;

}

void SettingsManager::setSeadriveRoot(const QString& seadrive_root)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    settings.setValue(kSeadriveRoot, seadrive_root);
    settings.endGroup();

}
#endif // _MSC_VER

#if defined(Q_OS_LINUX)
bool SettingsManager::getDataDir(QString *current_data_dir)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    if (!settings.contains(kCacheDir)) {
        return false;
    }

    *current_data_dir = settings.value(kCacheDir).toString();
    settings.endGroup();

    return true;
}

void SettingsManager::setDataDir(const QString& current_data_dir)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    settings.setValue(kCacheDir, current_data_dir);
    settings.endGroup();
}
#endif

void SettingsManager::writeSystemProxyInfo(const QUrl &url,
                                           const QString &file_path)
{
    QNetworkProxy proxy;
    bool use_proxy = getSystemProxyForUrl(url, &proxy);

    QString content;
    if (use_proxy) {
        QString type;
        if (proxy.type() == QNetworkProxy::HttpProxy ||
            proxy.type() == QNetworkProxy::HttpCachingProxy) {
            type = "http";
        } else {
            type = "socks";
        }
        QString json_content =
            "{\"type\": \"%1\", \"addr\": \"%2\", \"port\": %3, \"username\": "
            "\"%4\", \"password\": \"%5\"}";
        content = json_content.arg(type)
                      .arg(proxy.hostName())
                      .arg(proxy.port())
                      .arg(proxy.user())
                      .arg(proxy.password());
    } else {
        content = "{\"type\": \"none\"}";
    }

    QFile system_proxy_txt(file_path);
    if (!system_proxy_txt.open(QIODevice::WriteOnly)) {
        return;
    }

    system_proxy_txt.write(content.toUtf8().data());
}

void SettingsManager::checkSystemProxy()
{
    if (current_proxy_.type != SystemProxy) {
        // qDebug ("current proxy is not system proxy, return\n");
        return;
    }

    if (gui->accountManager()->activeAccounts().empty()) {
        return;
    }
    QUrl url = gui->accountManager()->activeAccounts().front().serverUrl;

    SystemProxyPoller *poller = new SystemProxyPoller(url);
    connect(poller, SIGNAL(systemProxyPolled(const QNetworkProxy &)), this,
            SLOT(onSystemProxyPolled(const QNetworkProxy &)));

    QThreadPool::globalInstance()->start(poller);
}


void SettingsManager::onSystemProxyPolled(const QNetworkProxy &system_proxy)
{
    if (current_proxy_.type != SystemProxy) {
        return;
    }
    if (last_system_proxy_ == system_proxy) {
        // qDebug ("system proxy not changed\n");
        return;
    }
    // qDebug ("system proxy changed\n");
    last_system_proxy_ = system_proxy;
    SeafileProxy proxy = SeafileProxy::fromQtNetworkProxy(system_proxy);
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i <  accounts.size(); i++) {
        SeafileRpcClient *rpc_client = gui->rpcClient(accounts.at(i).domainID());
        if (!rpc_client) {
            continue;
        }
        if (proxy.type == NoProxy) {
            // qDebug ("system proxy changed to no proxy\n");
            rpc_client->seafileSetConfig(kProxyType, "none");
        } else {
            writeProxyDetailsToDaemon(accounts.at(i).domainID(), proxy);
        }
    }
}

SystemProxyPoller::SystemProxyPoller(const QUrl &url) : url_(url)
{
}

#ifndef Q_OS_MAC
bool SettingsManager::getMigrateStatus()
{
    QSettings settings;
    bool finished;

    settings.beginGroup(kSettingsGroup);
    finished = settings.value(kMigrateStatus, false).toBool();
    settings.endGroup();

    return finished;
}

void SettingsManager::setMigrateStatus(bool finished)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kMigrateStatus, finished);
    settings.endGroup();
}
#endif

qint64 SettingsManager::getLastOpenSyncDialogTimestamp()
{
    QSettings settings;
    qint64 timestamp;

    settings.beginGroup(kSettingsGroup);
    timestamp = settings.value(kLastOpenSyncDialogTimestamp, 0).toLongLong();
    settings.endGroup();

    return timestamp;
}

void SettingsManager::setLastOpenSyncDialogTimestamp(qint64 timestamp)
{
    QSettings settings;
    settings.beginGroup(kSettingsGroup);
    settings.setValue(kLastOpenSyncDialogTimestamp, QVariant::fromValue(timestamp));
    settings.endGroup();
}

void SystemProxyPoller::run()
{
    QNetworkProxy proxy;
    bool use_proxy = getSystemProxyForUrl(url_, &proxy);
    if (!use_proxy) {
        proxy.setType(QNetworkProxy::NoProxy);
    }
    emit systemProxyPolled(proxy);
}
