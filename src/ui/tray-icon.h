#ifndef SEAFILE_CLIENT_TRAY_ICON_H
#define SEAFILE_CLIENT_TRAY_ICON_H

#include <QSystemTrayIcon>
#include <QHash>
#include <QQueue>

class QAction;
class QMenu;
class QMenuBar;

class Account;
class ApiError;
class LoginDialog;
class TrayNotificationManager;


class SeafileTrayIcon : public QSystemTrayIcon {
    Q_OBJECT

public:
    explicit SeafileTrayIcon(QObject *parent=0);

    enum TrayState {
        STATE_NONE = 0,
        STATE_DAEMON_UP,
        STATE_DAEMON_DOWN,
        STATE_DAEMON_AUTOSYNC_DISABLED,
        STATE_TRANSFER_1,
        STATE_TRANSFER_2,
        STATE_SERVERS_NOT_CONNECTED,
        STATE_HAVE_UNREAD_MESSAGE,
    };

    void start();

    TrayState state() const { return state_; }
    void setState(TrayState state, const QString& tip=QString());
    void rotate(bool start);

    void reloadTrayIcon();

    void showMessage(const QString& title,
                     const QString& message,
                     const QString& repo_id = QString(),
                     const QString& commit_id = QString(),
                     const QString& previous_commit_id = QString(),
                     MessageIcon icon = Information,
                     int millisecondsTimeoutHint = 10000);

    void setTransferRate(qint64 up_rate, qint64 down_rate);

public slots:
    void showSettingsWindow();
    void showLoginDialog();
    void showLoginDialog(const Account& account);
    void showAboutDialog();
    void onLoginDialogClosed();

private slots:
    void quitSeafile();
    void onActivated(QSystemTrayIcon::ActivationReason);
    void prepareContextMenu();
    void rotateTrayIcon();
    void refreshTrayIcon();
    void refreshTrayIconToolTip();
    void openHelp();
    void openSeafileFolder();
    void openLogDirectory();
    void about();
    void checkTrayIconMessageQueue();

    void onAccountItemClicked();

    void logoutAccount();
    void onLogoutDeviceRequestSuccess();
    void onLogoutDeviceRequestFailed(const ApiError& error);

    void deleteAccount();

    // only used on windows
    void onMessageClicked();

private:
    Q_DISABLE_COPY(SeafileTrayIcon)

    void createActions();
    void createContextMenu();
    void createGlobalMenuBar();

    QIcon stateToIcon(TrayState state);
    QIcon getIcon(const QString& name);

    QMenu *context_menu_;
    QMenu *help_menu_;
    QMenu *account_menu_;

    QMenu *global_menu_;
    QMenu *dock_menu_;
    QMenuBar *global_menubar_;

    // Actions for tray icon menu
    QAction *quit_action_;
    QAction *settings_action_;
    QAction *login_action_;
    QAction *open_seafile_folder_action_;
    QAction *open_log_directory_action_;
    QAction *view_unread_seahub_notifications_action_;

    QAction *about_action_;
    QAction *open_help_action_;


    QTimer *rotate_timer_;
    QTimer *refresh_timer_;
    int nth_trayicon_;
    int rotate_counter_;
    bool auto_sync_;

    TrayState state_;

    QString repo_id_;
    QString commit_id_;
    QString previous_commit_id_;

    QHash<QString, QIcon> icon_cache_;

    struct TrayMessage {
        QString title;
        QString message;
        MessageIcon icon;
        QString repo_id;
        QString commit_id;
        QString previous_commit_id;
    };

    // Use a queue to gurantee each tray notification message would be
    // displayed at least several seconds.
    QQueue<TrayMessage> pending_messages_;
    qint64 next_message_msec_;

    LoginDialog *login_dlg_;

    TrayNotificationManager *tnm;

    QAction *transfer_rate_display_action_;
    qint64 up_rate_;
    qint64 down_rate_;
};

#endif // SEAFILE_CLIENT_TRAY_ICON_H
