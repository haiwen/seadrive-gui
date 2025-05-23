#ifndef SEAFILE_CLIENT_SETTINGS_MANAGER_H
#define SEAFILE_CLIENT_SETTINGS_MANAGER_H

#include <QObject>
#include <QRunnable>
#include <QUrl>
#include <QNetworkProxy>

/**
 * Settings Manager handles seafile client user settings & preferences
 */
class QTimer;

class SettingsManager : public QObject {
    Q_OBJECT

public:
    enum ProxyType {
        NoProxy = 0,
        HttpProxy = 1,
        SocksProxy = 2,
        SystemProxy = 3
    };

    struct SeafileProxy {
        ProxyType type;

        QString host;
        int port;
        QString username;
        QString password;

        SeafileProxy(ProxyType _type = NoProxy,
                     const QString _host = QString(),
                     int _port = 0,
                     const QString& _username = QString(),
                     const QString& _password = QString())
            : type(_type),
              host(_host),
              port(_port),
              username(_username),
              password(_password)
        {
        }

        void toQtNetworkProxy(QNetworkProxy *proxy) const;

        bool operator==(const SeafileProxy& rhs) const;
        bool operator!=(const SeafileProxy& rhs) const { return !(*this == rhs); };

        static SeafileProxy fromQtNetworkProxy(const QNetworkProxy& proxy);
    };

    SettingsManager();

    void loadSettings();
    void loadProxySettings();
    void applyProxySettings();

    bool notify() { return bubbleNotifycation_; }
    bool autoStart() { return autoStart_; }
    unsigned int maxDownloadRatio() { return maxDownloadRatio_; }
    unsigned int maxUploadRatio() { return maxUploadRatio_; }
    bool syncExtraTempFile() { return sync_extra_temp_file_; }

    void getProxy(QNetworkProxy *proxy) const;
    SeafileProxy getProxy() const { return current_proxy_; };
    void setProxy(const SeafileProxy& proxy);

    void setNotify(bool notify);
    bool getNotify();
    void setAutoStart(bool autoStart);
    void setMaxDownloadRatio(unsigned int ratio);
    unsigned int geteMaxDownloadRatio();
    void setMaxUploadRatio(unsigned int ratio);
    unsigned int geteMaxUploadRatio();
    void setSyncExtraTempFile(bool sync);
    bool getSyncExtraTempFile();
    void setSearchEnabled(bool enabled);
    bool getSearchEnabled();

    void setCheckLatestVersionEnabled(bool enabled);
    bool isCheckLatestVersionEnabled();

    void setHttpSyncCertVerifyDisabled(bool disabled);
    bool getHttpSyncCertVerifyDisabled();

    void setDeleteConfirmThreshold(int value);
    int getDeleteConfirmThreshold();

    QString getComputerName();
    void setComputerName(const QString& computerName);

    void setCacheCleanIntervalMinutes(int interval);
    int getCacheCleanIntervalMinutes();

    void setCacheSizeLimitGB(int limit);
    int getCacheSizeLimitGB();

    QString getLastShibUrl();
    void setLastShibUrl(const QString& url);

#ifdef Q_OS_WIN32
    void setShellExtensionEnabled(bool enabled);
    bool shellExtensionEnabled() const { return shell_ext_enabled_; }
#endif // Q_OS_WIN32

#if defined(_MSC_VER)
    bool getSeadriveRoot(QString *seadrive_root);
    void setSeadriveRoot(const QString& seadrive_root);
#endif

#if defined(Q_OS_LINUX)
    bool getDataDir(QString *current_data_dir);
    void setDataDir(const QString& current_data_dir);
#endif

#if defined(Q_OS_MAC)
    bool getHideWindowsIncompatibilityPathMsg();
    void setHideWindowsIncompatibilityPathMsg(bool enabled);
#else
    bool getMigrateStatus();
    void setMigrateStatus(bool finished);
#endif
public:

    // Remove all settings from system when uninstall
    static void removeAllSettings();
    // Write the system proxy information, to be read by seadrive daemon.
    void writeSystemProxyInfo(const QUrl& url, const QString& file_path);

    void writeProxySettingsToDaemon(const QString& domain_id, const SeafileProxy& proxy);

    void writeSettingsToDaemon();

private slots:
    void checkSystemProxy();
    void onSystemProxyPolled(const QNetworkProxy& proxy);

private:
    Q_DISABLE_COPY(SettingsManager)

    void writeProxySettings(const SeafileProxy& proxy);
    void writeProxyDetailsToDaemon(const QString& domain_id, const SeafileProxy& proxy);

    bool auto_sync_;
    bool bubbleNotifycation_;
    bool autoStart_;
    bool sync_extra_temp_file_;
    unsigned int maxDownloadRatio_;
    unsigned int maxUploadRatio_;
    bool verify_http_sync_cert_disabled_;
    bool shell_ext_enabled_;

    // proxy settings
    SeafileProxy current_proxy_;
    QNetworkProxy last_system_proxy_;

    int cache_clean_limit_minutes_;
    int cache_size_limit_gb_;

    int delete_confirm_threshold_;

    QTimer *check_system_proxy_timer_;
};


// Use to periodically reading the current system proxy.
class SystemProxyPoller : public QObject, public QRunnable {
    Q_OBJECT
public:
    SystemProxyPoller(const QUrl& url);
    void run();

signals:
    void systemProxyPolled(const QNetworkProxy& proxy);

private:
    QUrl url_;
};


#endif // SEAFILE_CLIENT_SETTINGS_MANAGER_H
