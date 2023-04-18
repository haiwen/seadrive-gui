#ifndef SEAFILE_SEADRIVE_GUI_H
#define SEAFILE_SEADRIVE_GUI_H

#include <QObject>
#include <QVariant>
#include <QMessageBox>
#include <QProcess>
#include <QTimer>

#include "rpc/rpc-server.h"

class DaemonManager;
class SeafileRpcClient;
class AccountManager;
class MessageListener;
class SeafileTrayIcon;
class SettingsManager;
class SettingsDialog;
class CertsManager;
class MessagePoller;
class AboutDialog;
class InitSyncDialog;
class FileProviderManager;

/**
 * The central class of seafile-client
 */
class SeadriveGui : public QObject {
    Q_OBJECT

public:
    SeadriveGui(bool dev_mode);
    ~SeadriveGui();

    void refreshQss();

    void messageBox(const QString& msg, QWidget *parent=0);
    void warningBox(const QString& msg, QWidget *parent=0);
    bool yesOrNoBox(const QString& msg, QWidget *parent=0, bool default_val=true);
    bool detailedYesOrNoBox(const QString& msg, const QString& detailed_text, QWidget *parent, bool default_val=true);
    QMessageBox::StandardButton yesNoCancelBox(const QString& msg,
                                               QWidget *parent,
                                               QMessageBox::StandardButton default_btn);
    bool yesOrCancelBox(const QString& msg, QWidget *parent, bool default_ok);
    bool deletingConfirmationBox(const QString& text, const QString& info);

    // Show error in a messagebox and exit
    void errorAndExit(const QString& error);
    void restartApp();

    // We generate a unique id for this computer in ~/.seadrive/uuid, which can
    // be reused until the software is uninstalled and user data removed.
    QString getUniqueClientId();

    // Read preconfigure settings
    QVariant readPreconfigureEntry(const QString& key, const QVariant& default_value = QVariant());
    // ExpandedVars String
    QString readPreconfigureExpandedString(const QString& key, const QString& default_value = QString());

    // Accessors.
    bool isDevMode() const { return dev_mode_; }

#if defined(Q_OS_WIN32)
    QString seadriveRoot() const;
#endif

    SeafileTrayIcon *trayIcon() { return tray_icon_; }

    DaemonManager *daemonManager() { return daemon_mgr_; }

    AccountManager *accountManager() { return account_mgr_; }

    SeafileRpcClient *rpcClient() { return rpc_client_; }

    SettingsManager *settingsManager() { return settings_mgr_; }

    SettingsDialog *settingsDialog() { return settings_dlg_; }

    AboutDialog *aboutDialog() { return about_dlg_; }

    MessagePoller * messagePoller() { return message_poller_; }

    InitSyncDialog *initSyncDialog() { return init_sync_dlg_; }

    FileProviderManager *fileProviderManager() { return file_provider_mgr_; }

    // CertsManager *certsManager() { return certs_mgr_; }

    bool started() { return started_; }
    bool inExit() { return in_exit_; }
    qint64 startupTime() const { return startup_time_; }

public slots:
    void start();

private slots:
    void onAboutToQuit();
    void onDaemonStarted();
    void updateAccountToDaemon();

    void onDaemonRestarted();

#ifndef Q_OS_WIN32
    void connectDaemon();
#endif

private:
    Q_DISABLE_COPY(SeadriveGui)

    bool initLog();

    bool loadQss(const QString& path);

    void loginAccounts();

#if defined(Q_OS_MAC)
    void logoutAccountsFromDaemon();
#endif

    bool dev_mode_;

    SeafileTrayIcon *tray_icon_;

    DaemonManager *daemon_mgr_;

    AccountManager *account_mgr_;

    SeafileRpcClient *rpc_client_;

    SettingsManager *settings_mgr_;

    SettingsDialog *settings_dlg_;

    AboutDialog *about_dlg_;

    MessagePoller *message_poller_;

    InitSyncDialog *init_sync_dlg_;

    FileProviderManager *file_provider_mgr_;

    // SettingsDialog *settings_dialog_;

    // CertsManager *certs_mgr_;

    bool started_;

    bool in_exit_;

    QString style_;

    bool first_use_;

    QString disk_letter_;
#if defined(_MSC_VER)

    QString seadrive_root_;

#endif // _MSC_VER

    qint64 startup_time_;

    QTimer connect_daemon_timer_;

#if defined(Q_OS_MAC)
    bool notified_start_extension_;
    int connect_daemon_retry_;
#endif
};

/**
 * The global SeadriveGui object
 */
extern SeadriveGui *gui;

#define STR(s)     #s
#define STRINGIZE(x) STR(x)

#endif // SEAFILE_SEADRIVE_GUI_H
