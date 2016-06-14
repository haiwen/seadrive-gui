#ifndef SEAFILE_SEADRIVE_GUI_H
#define SEAFILE_SEADRIVE_GUI_H

#include <QObject>
#include <QVariant>
#include <QMessageBox>

class QMainWindow;

class Configurator;
class DaemonManager;
class SeafileRpcClient;
class AccountManager;
class MainWindow;
class MessageListener;
class SeafileTrayIcon;
class SettingsManager;
class SettingsDialog;
class CertsManager;

/**
 * The central class of seafile-client
 */
class SeadriveGui : QObject {
    Q_OBJECT

public:
    SeadriveGui();
    ~SeadriveGui();

    void start();

    void refreshQss();

    void messageBox(const QString& msg, QWidget *parent=0);
    void warningBox(const QString& msg, QWidget *parent=0);
    bool yesOrNoBox(const QString& msg, QWidget *parent=0, bool default_val=true);
    bool detailedYesOrNoBox(const QString& msg, const QString& detailed_text, QWidget *parent, bool default_val=true);
    QMessageBox::StandardButton yesNoCancelBox(const QString& msg,
                                               QWidget *parent,
                                               QMessageBox::StandardButton default_btn);
    bool yesOrCancelBox(const QString& msg, QWidget *parent, bool default_ok);

    // Show error in a messagebox and exit
    void errorAndExit(const QString& error);
    void restartApp();

    // Read preconfigure settings
    QVariant readPreconfigureEntry(const QString& key, const QVariant& default_value = QVariant());
    // ExpandedVars String
    QString readPreconfigureExpandedString(const QString& key, const QString& default_value = QString());

    // Accessors.

    // Get the seadrive folder. It's ~/seadrive on windows and ~/.seadrive on unix.
    QString seadriveDir() const;

    // Get the seadrive daemon data dir. The "data" subfolder of seadrive dir.
    QString seadriveDataDir() const;

    // Get the seadrive logs dir. The "logs" subfolder of seadrive dir.
    QString logsDir() const;

    SeafileTrayIcon *trayIcon() { return tray_icon_; }

    DaemonManager *daemonManager() { return daemon_mgr_; }

    // AccountManager *accountManager() { return account_mgr_; }

    // SeafileRpcClient *rpcClient() { return rpc_client_; }


    // Configurator *configurator() { return configurator_; }

    // MainWindow *mainWindow() { return main_win_; }

    // SettingsDialog *settingsDialog() { return settings_dialog_; }

    // SettingsManager *settingsManager() { return settings_mgr_; }

    // CertsManager *certsManager() { return certs_mgr_; }

    bool started() { return started_; }
    bool inExit() { return in_exit_; }

private slots:
    void onAboutToQuit();
    void onDaemonStarted();

private:
    Q_DISABLE_COPY(SeadriveGui)

    void initLog();

    bool loadQss(const QString& path);

    SeafileTrayIcon *tray_icon_;

    DaemonManager *daemon_mgr_;

    // Configurator *configurator_;

    // AccountManager *account_mgr_;


    QMainWindow* main_win_;

    // SeafileRpcClient *rpc_client_;

    // MessageListener *message_listener_;

    // SettingsDialog *settings_dialog_;

    // SettingsManager *settings_mgr_;

    // CertsManager *certs_mgr_;

    bool started_;

    bool in_exit_;

    QString style_;

    bool is_pro_;
};

/**
 * The global SeadriveGui object
 */
extern SeadriveGui *gui;

#define STR(s)     #s
#define STRINGIZE(x) STR(x)

#endif // SEAFILE_SEADRIVE_GUI_H
