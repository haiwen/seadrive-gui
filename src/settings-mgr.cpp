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

#ifdef HAVE_FINDER_SYNC_SUPPORT
#include "finder-sync/finder-sync.h"
#endif

#include "settings-mgr.h"

namespace
{
const char *kCheckLatestVersion = "checkLatestVersion";
const char *kBehaviorGroup = "Behavior";
#ifdef Q_OS_WIN32
const char *kDiskLetter = "diskLetter";
#endif

// const char *kDefaultLibraryAlreadySetup = "defaultLibraryAlreadySetup";
// const char *kStatusGroup = "Status";

const char *kSettingsGroup = "Settings";
const char *kComputerName = "computerName";
const char *kCacheDir = "cacheDir";
#ifdef HAVE_FINDER_SYNC_SUPPORT
const char *kFinderSync = "finderSync";
#endif // HAVE_FINDER_SYNC_SUPPORT
const char *kLastShibUrl = "lastShiburl";

const char *kUseProxy = "use_proxy";
const char *kUseSystemProxy = "use_system_proxy";
const char *kProxyType = "proxy_type";
const char *kProxyAddr = "proxy_addr";
const char *kProxyPort = "proxy_port";
const char *kProxyUsername = "proxy_username";
const char *kProxyPassword = "proxy_password";

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
      current_session_access_(false),
      current_proxy_(SeafileProxy()),
      cache_clean_limit_minutes_(10),
      cache_size_limit_gb_(10)
{
    check_system_proxy_timer_ = new QTimer(this);
    connect(check_system_proxy_timer_, SIGNAL(timeout()), this, SLOT(checkSystemProxy()));
}

void SettingsManager::loadSettings()
{
    QString str;
    int value;

    if (gui->rpcClient()->seafileGetConfig("notify_sync", &str) >= 0)
        bubbleNotifycation_ = (str == "off") ? false : true;

    if (gui->rpcClient()->seafileGetConfigInt("download_limit", &value) >= 0)
        maxDownloadRatio_ = value >> 10;

    if (gui->rpcClient()->seafileGetConfigInt("upload_limit", &value) >= 0)
        maxUploadRatio_ = value >> 10;

    if (gui->rpcClient()->seafileGetConfig("sync_extra_temp_file",
                                                  &str) >= 0)
        sync_extra_temp_file_ = (str == "true") ? true : false;

    if (gui->rpcClient()->seafileGetConfig("disable_verify_certificate",
                                                  &str) >= 0)
        verify_http_sync_cert_disabled_ = (str == "true") ? true : false;

    if (gui->rpcClient()->seafileGetConfig("current_session_access",
                                                  &str) >= 0)
        current_session_access_ = (str == "true") ? true : false;

    if (gui->rpcClient()->getCacheSizeLimitGB(&value)) {
        cache_size_limit_gb_ = qMax(1, value);
    }

    if (gui->rpcClient()->getCacheCleanIntervalMinutes(&value)) {
        cache_clean_limit_minutes_ = qMax(1, value);
    }

    loadProxySettings();
    applyProxySettings();

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
    RegElement reg(HKEY_CURRENT_USER, softwareSeaDrive(), "ShellExtDisabled",
                   "");
    shell_ext_enabled_ = !reg.exists();
#endif
}

void SettingsManager::loadProxySettings()
{
    SeafileProxy proxy;

    QString use_proxy;
    gui->rpcClient()->seafileGetConfig(kUseProxy, &use_proxy);
    if (use_proxy != "true") {
        return;
    }
    QString use_system_proxy;
    gui->rpcClient()->seafileGetConfig(kUseSystemProxy, &use_system_proxy);
    if (use_system_proxy == "true") {
        current_proxy_.type = SystemProxy;
        return;
    }

    QString proxy_type;
    QString proxy_host;
    int proxy_port;
    QString proxy_username;
    QString proxy_password;

    if (gui->rpcClient()->seafileGetConfig(kProxyAddr, &proxy_host) <
        0) {
        return;
    }
    if (gui->rpcClient()->seafileGetConfigInt(kProxyPort, &proxy_port) <
        0) {
        return;
    }
    if (gui->rpcClient()->seafileGetConfig(kProxyType, &proxy_type) <
        0) {
        return;
    }
    if (proxy_type == "http") {
        if (gui->rpcClient()->seafileGetConfig(kProxyUsername,
                                                      &proxy_username) < 0) {
            return;
        }
        if (gui->rpcClient()->seafileGetConfig(kProxyPassword,
                                                      &proxy_password) < 0) {
            return;
        }
        proxy.type = HttpProxy;
        proxy.host = proxy_host;
        proxy.port = proxy_port;
        proxy.username = proxy_username;
        proxy.password = proxy_password;

    } else if (proxy_type == "socks") {
        proxy.type = SocksProxy;
        proxy.host = proxy_host;
        proxy.port = proxy_port;
    } else if (!proxy_type.isEmpty()) {
        qWarning("Unsupported proxy_type %s", proxy_type.toUtf8().data());
        return;
    }

    current_proxy_ = proxy;
}

void SettingsManager::setNotify(bool notify)
{
    if (bubbleNotifycation_ != notify) {
        if (gui->rpcClient()->seafileSetConfig(
                "notify_sync", notify ? "on" : "off") < 0) {
            return;
        }
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

void SettingsManager::setMaxDownloadRatio(unsigned int ratio)
{
    if (maxDownloadRatio_ != ratio) {
        if (gui->rpcClient()->setDownloadRateLimit(ratio << 10) < 0) {
            return;
        }
        maxDownloadRatio_ = ratio;
    }
}

void SettingsManager::setMaxUploadRatio(unsigned int ratio)
{
    if (maxUploadRatio_ != ratio) {
        if (gui->rpcClient()->setUploadRateLimit(ratio << 10) < 0) {
            return;
        }
        maxUploadRatio_ = ratio;
    }
}

void SettingsManager::setCacheCleanIntervalMinutes(int interval)
{
    if (cache_clean_limit_minutes_ != interval) {
        if (!gui->rpcClient()->setCacheCleanIntervalMinutes(interval)) {
            return;
        }
        cache_clean_limit_minutes_ = interval;
    }
}

void SettingsManager::setCacheSizeLimitGB(int limit)
{
    if (cache_size_limit_gb_ != limit) {
        if (!gui->rpcClient()->setCacheSizeLimitGB(limit)) {
            return;
        }
        cache_size_limit_gb_ = limit;
    }
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
    if (sync_extra_temp_file_ != sync) {
        if (gui->rpcClient()->seafileSetConfig(
                "sync_extra_temp_file", sync ? "true" : "false") < 0) {
            return;
        }
        sync_extra_temp_file_ = sync;
    }
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
    if (type == HttpProxy && !username.isEmpty() && !password.isEmpty()) {
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
        return host == rhs.host && port == rhs.port;
    }
}

void SettingsManager::setProxy(const SeafileProxy &proxy)
{
    if (proxy == current_proxy_) {
        return;
    }
    current_proxy_ = proxy;

    writeProxySettingsToDaemon(proxy);
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

void SettingsManager::writeProxySettingsToDaemon(const SeafileProxy &proxy)
{
    SeafileRpcClient *rpc = gui->rpcClient();
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

    writeProxyDetailsToDaemon(proxy);
}

void SettingsManager::writeProxyDetailsToDaemon(const SeafileProxy& proxy)
{
    Q_ASSERT(proxy.type != NoProxy && proxy.type != SystemProxy);
    SeafileRpcClient *rpc = gui->rpcClient();
    QString type = proxy.type == HttpProxy ? "http" : "socks";
    rpc->seafileSetConfig(kProxyType, type);
    rpc->seafileSetConfig(kProxyAddr, proxy.host.toUtf8().data());
    rpc->seafileSetConfigInt(kProxyPort, proxy.port);
    if (type == "http") {
        rpc->seafileSetConfig(kProxyUsername, proxy.username.toUtf8().data());
        rpc->seafileSetConfig(kProxyPassword, proxy.password.toUtf8().data());
    }
}

void SettingsManager::setHttpSyncCertVerifyDisabled(bool disabled)
{
    if (verify_http_sync_cert_disabled_ != disabled) {
        if (gui->rpcClient()->seafileSetConfig(
                "disable_verify_certificate", disabled ? "true" : "false") < 0) {
            return;
        }
        verify_http_sync_cert_disabled_ = disabled;
    }
}

void SettingsManager::setCurrentUserAccess(bool disabled)
{
    if (current_session_access_ != disabled) {
        if (gui->rpcClient()->seafileSetConfig(
                "current_session_access", disabled ? "true" : "false") < 0) {
            return;
        }
        current_session_access_ = disabled;
    }
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

bool SettingsManager::getDiskLetter(QString *disk_letter)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    if (!settings.contains(kDiskLetter)) {
        return false;
    }

    *disk_letter = settings.value(kDiskLetter, true).toString();
    settings.endGroup();

    return true;
}

void SettingsManager::setDiskLetter(const QString& disk_letter)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    settings.setValue(kDiskLetter, disk_letter);
    settings.endGroup();
}


#endif // Q_OS_WIN32

bool SettingsManager::getCacheDir(QString *current_cache_dir)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    if (!settings.contains(kCacheDir)) {
        return false;
    }

    *current_cache_dir = settings.value(kCacheDir).toString();
    settings.endGroup();

    return true;
}

void SettingsManager::setCacheDir(const QString& current_cache_dir)
{
    QSettings settings;

    settings.beginGroup(kSettingsGroup);
    settings.setValue(kCacheDir, current_cache_dir);
    settings.endGroup();
}

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

    const Account &account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    SystemProxyPoller *poller = new SystemProxyPoller(account.serverUrl);
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
    if (proxy.type == NoProxy) {
        // qDebug ("system proxy changed to no proxy\n");
        gui->rpcClient()->seafileSetConfig(kProxyType, "none");
    } else {
        writeProxyDetailsToDaemon(proxy);
    }
}

SystemProxyPoller::SystemProxyPoller(const QUrl &url) : url_(url)
{
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
