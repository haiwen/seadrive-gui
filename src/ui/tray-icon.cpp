#include <glib.h>

#include <QtGlobal>

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QtWidgets>
#else
#include <QtGui>
#endif
#include <QApplication>
#include <QDesktopServices>
#include <QSet>
#include <QDebug>
#include <QMenuBar>
#include <QRunnable>

#include "utils/utils.h"
#include "utils/utils-mac.h"
#include "utils/file-utils.h"
#include "src/ui/settings-dialog.h"
#include "src/ui/login-dialog.h"
#include "src/ui/init-sync-dialog.h"
#include "src/ui/about-dialog.h"
#include "api/api-error.h"
#include "api/requests.h"
#include "seadrive-gui.h"
#include "account-mgr.h"
#include "rpc/rpc-client.h"

#include "tray-icon.h"

#if defined(Q_OS_MAC)
#include "traynotificationmanager.h"
// QT's platform apis
// http://qt-project.org/doc/qt-4.8/exportedfunctions.html
extern void qt_mac_set_dock_menu(QMenu *menu);
#endif

#include "utils/utils-mac.h"
#include "utils/utils-win.h"

#if defined(Q_OS_LINUX)
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#endif

namespace {

const int kRefreshInterval = 1000;
const int kRotateTrayIconIntervalMilli = 250;
const int kMessageDisplayTimeMSecs = 5000;

#ifdef Q_OS_MAC
void darkmodeWatcher(bool /*new Value*/) {
    gui->trayIcon()->reloadTrayIcon();
}
#endif

QString translateTransferRate(int rate)
{
    QString unit;
    QString display_rate;
    double kbps = ((double)rate) / 1024;
    if (kbps >= 1024) {
        unit = "MB/s";
        double mbps = kbps / 1024;
        if (mbps < 10) {
            display_rate = QString::number(mbps, 'f', 1);
        } else {
            display_rate = QString::number(int(mbps));
        }
    }
    else {
        display_rate = kbps;
        unit = "kB/s";
        display_rate = QString::number(int(kbps));
    }

    return QString("%1 %2")
        .arg(display_rate)
        .arg(unit);
}

} // namespace

SeafileTrayIcon::SeafileTrayIcon(QObject *parent)
    : QSystemTrayIcon(parent),
      nth_trayicon_(0),
      rotate_counter_(0),
      state_(STATE_NONE),
      next_message_msec_(0),
      login_dlg_(nullptr),
      up_rate_(0),
      down_rate_(0)
{
    setState(STATE_DAEMON_UP);
    rotate_timer_ = new QTimer(this);
    connect(rotate_timer_, SIGNAL(timeout()), this, SLOT(rotateTrayIcon()));

    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, SIGNAL(timeout()), this, SLOT(refreshTrayIcon()));
    connect(refresh_timer_, SIGNAL(timeout()), this, SLOT(refreshTrayIconToolTip()));
#if !defined(Q_OS_LINUX)
    connect(refresh_timer_, SIGNAL(timeout()), this, SLOT(checkTrayIconMessageQueue()));
#endif

    createActions();
    createContextMenu();

    connect(this, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(onActivated(QSystemTrayIcon::ActivationReason)));

#if !defined(Q_OS_LINUX)
    connect(this, SIGNAL(messageClicked()),
            this, SLOT(onMessageClicked()));
#endif

    hide();

    createGlobalMenuBar();
#if defined(Q_OS_MAC)
    tnm = new TrayNotificationManager(this);
#endif
}

void SeafileTrayIcon::start()
{
    show();
    refresh_timer_->start(kRefreshInterval);
#if defined(Q_OS_MAC)
    utils::mac::set_darkmode_watcher(&darkmodeWatcher);
#endif
}

void SeafileTrayIcon::createActions()
{
    // The text would be set at the menu open time.
    transfer_rate_display_action_ = new QAction("", this);

    quit_action_ = new QAction(tr("&Quit"), this);
    connect(quit_action_, SIGNAL(triggered()), this, SLOT(quitSeafile()));

    settings_action_ = new QAction(tr("Settings"), this);
    connect(settings_action_, SIGNAL(triggered()), this, SLOT(showSettingsWindow()));

    // login_action_ = new QAction(tr("Add another account"), this);
    // connect(login_action_, SIGNAL(triggered()), this, SLOT(showLoginDialog()));

    open_seafile_folder_action_ = new QAction(tr("Open %1 &folder").arg(getBrand()), this);
    open_seafile_folder_action_->setStatusTip(tr("open %1 folder").arg(getBrand()));
    connect(open_seafile_folder_action_, SIGNAL(triggered()), this, SLOT(openSeafileFolder()));

    open_log_directory_action_ = new QAction(tr("Open &logs folder"), this);
    open_log_directory_action_->setStatusTip(tr("open %1 log folder").arg(getBrand()));
    connect(open_log_directory_action_, SIGNAL(triggered()), this, SLOT(openLogDirectory()));

    about_action_ = new QAction(tr("&About"), this);
    about_action_->setStatusTip(tr("Show the application's About box"));
//    connect(about_action_, SIGNAL(triggered()), this, SLOT(about()));
    connect(about_action_, SIGNAL(triggered()), this, SLOT(showAboutDialog()));

    open_help_action_ = new QAction(tr("&Online help"), this);
    open_help_action_->setStatusTip(tr("open %1 online help").arg(getBrand()));
    connect(open_help_action_, SIGNAL(triggered()), this, SLOT(openHelp()));
}

void SeafileTrayIcon::createContextMenu()
{
    // help_menu_ = new QMenu(tr("Help"), NULL);
    // help_menu_->addAction(about_action_);
    // help_menu_->addAction(open_help_action_);

    context_menu_ = new QMenu(NULL);
    // context_menu_->addAction(view_unread_seahub_notifications_action_);

    context_menu_->addAction(transfer_rate_display_action_);
    context_menu_->addSeparator();

    context_menu_->addAction(open_seafile_folder_action_);

    context_menu_->addAction(open_seafile_folder_action_);
    context_menu_->addAction(open_log_directory_action_);
    // context_menu_->addAction(login_action_);
    context_menu_->addAction(settings_action_);
    // context_menu_->addMenu(help_menu_);

    context_menu_->addSeparator();
    account_menu_ = new QMenu(tr("Accounts"), NULL);
    context_menu_->addMenu(account_menu_);

    context_menu_->addSeparator();
    context_menu_->addAction(about_action_);
    context_menu_->addAction(open_help_action_);

    context_menu_->addSeparator();
    context_menu_->addAction(quit_action_);

    setContextMenu(context_menu_);
    connect(context_menu_, SIGNAL(aboutToShow()), this, SLOT(prepareContextMenu()));
}

void SeafileTrayIcon::prepareContextMenu()
{
    const std::vector<Account>& accounts = gui->accountManager()->accounts();

    // if (rotate_timer_->isActive()) {
    //     transfer_rate_display_action_->setVisible(true);
    // } else {
    //     transfer_rate_display_action_->setVisible(false);
    // }

    // Remove all menu items
    account_menu_->clear();

    if (!accounts.empty()) {
        for (size_t i = 0, n = accounts.size(); i < n; i++) {
            const Account &account = accounts[i];
            QString text = account.username + "(" + account.serverUrl.host() + ")";
            if (!account.isValid()) {
                text += ", " + tr("not logged in");
            }
            QMenu *submenu = new QMenu(text, account_menu_);
            if (i == 0) {
                submenu->setIcon(QIcon(":/images/account-checked.png"));
            } else {
                submenu->setIcon(QIcon(":/images/account-else.png"));
            }

            QAction *submenu_action = submenu->menuAction();
            submenu_action->setData(QVariant::fromValue(account));
            connect(submenu_action, SIGNAL(triggered()), this, SLOT(onAccountItemClicked()));

            QAction *action = new QAction(tr("Choose"), submenu);
            action->setIcon(QIcon(":/images/account-checked.png"));
            action->setIconVisibleInMenu(true);
            action->setData(QVariant::fromValue(account));
            connect(action, SIGNAL(triggered()), this, SLOT(onAccountItemClicked()));

            submenu->addAction(action);
            submenu->setDefaultAction(action);

            // QAction *account_settings_action = new QAction(tr("Account settings"), this);
            // account_settings_action->setIcon(QIcon(":/images/account-settings.png"));
            // account_settings_action->setIconVisibleInMenu(true);
            // account_settings_action->setData(QVariant::fromValue(account));
            // connect(account_settings_action, SIGNAL(triggered()), this, SLOT(editAccountSettings()));
            // submenu->addAction(account_settings_action);

            if (account.isValid()) {
                QAction *logout_action = new QAction(this);
                logout_action->setIcon(QIcon(":/images/logout.png"));
                logout_action->setIconVisibleInMenu(true);
                logout_action->setData(QVariant::fromValue(account));
                connect(logout_action, SIGNAL(triggered()), this, SLOT(logoutAccount()));
                logout_action->setText(tr("Logout"));
                submenu->addAction(logout_action);
            }

            QAction *delete_account_action = new QAction(tr("Delete"), this);
            delete_account_action->setIcon(QIcon(":/images/delete-account.png"));
            delete_account_action->setIconVisibleInMenu(true);
            delete_account_action->setData(QVariant::fromValue(account));
            connect(delete_account_action, SIGNAL(triggered()), this, SLOT(deleteAccount()));
            submenu->addAction(delete_account_action);

            account_menu_->addMenu(submenu);
        }

        account_menu_->addSeparator();
    }

    login_action_ = new QAction(tr("Add an account"), this);
    login_action_->setIcon(QIcon(":/images/add-account.png"));
    login_action_->setIconVisibleInMenu(true);
    connect(login_action_, SIGNAL(triggered()), this, SLOT(showLoginDialog()));
    account_menu_->addAction(login_action_);
}

void SeafileTrayIcon::createGlobalMenuBar()
{
    // support it only on mac os x currently
    // TODO: destroy the objects when seafile closes
#ifdef Q_OS_MAC
    // create qmenu used in menubar and docker menu
    global_menu_ = new QMenu(tr("File"));
    global_menu_->addAction(open_seafile_folder_action_);
    global_menu_->addAction(open_log_directory_action_);
    global_menu_->addSeparator();

    global_menubar_ = new QMenuBar(0);
    global_menubar_->addMenu(global_menu_);
    // TODO fix the line below which crashes under qt5.4.0
    //global_menubar_->addMenu(help_menu_);
    global_menubar_->setNativeMenuBar(true);
    qApp->setAttribute(Qt::AA_DontUseNativeMenuBar, false);

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
    global_menu_->setAsDockMenu(); // available after qt5.2.0
#else
    qt_mac_set_dock_menu(global_menu_); // deprecated in latest qt
#endif
    // create QMenuBar that has no parent, so we can share the global menubar

#endif // Q_OS_MAC
}

void SeafileTrayIcon::rotate(bool start)
{
    /* tray icon should not be refreshed on Gnome according to their guidelines */
    const char *env = g_getenv("DESKTOP_SESSION");
    if (env && strcmp(env, "gnome") == 0) {
        return;
    }

    if (start) {
        rotate_counter_ = 0;
        if (!rotate_timer_->isActive()) {
            nth_trayicon_ = 0;
            rotate_timer_->start(kRotateTrayIconIntervalMilli);
        }
    } else {
        rotate_timer_->stop();
    }
}

void SeafileTrayIcon::showMessage(const QString &title,
                                  const QString &message,
                                  const QString &repo_id,
                                  const QString &commit_id,
                                  const QString &previous_commit_id,
                                  MessageIcon icon,
                                  int millisecondsTimeoutHint)
{
#if defined(Q_OS_LINUX)
    repo_id_ = repo_id;
    Q_UNUSED(icon);
    QVariantMap hints;
    QString brand = getBrand();
    hints["resident"] = QVariant(true);
    hints["desktop-entry"] = QVariant(brand);
    QList<QVariant> args = QList<QVariant>() << brand << quint32(0) << brand
                                             << title << message << QStringList () << hints << qint32(-1);
    QDBusMessage method = QDBusMessage::createMethodCall("org.freedesktop.Notifications","/org/freedesktop/Notifications", "org.freedesktop.Notifications", "Notify");
    method.setArguments(args);
    QDBusConnection::sessionBus().asyncCall(method);
#else
    TrayMessage msg;
    msg.title = title;
    msg.message = message;
    msg.icon = icon;
    msg.repo_id = repo_id;
    msg.commit_id = commit_id;
    msg.previous_commit_id = previous_commit_id;
    pending_messages_.enqueue(msg);
#endif
}

void SeafileTrayIcon::rotateTrayIcon()
{
    if (rotate_counter_ >= 8) {
        rotate_timer_->stop();
        setState (STATE_DAEMON_UP);
        return;
    }

    TrayState states[] = { STATE_TRANSFER_1, STATE_TRANSFER_2 };
    int index = nth_trayicon_ % 2;
    setIcon(stateToIcon(states[index]));

    nth_trayicon_++;
    rotate_counter_++;
}

void SeafileTrayIcon::setState(TrayState state, const QString& tip)
{
    if (state_ == state) {
        return;
    }

    QString tool_tip = tip.isEmpty() ? getBrand() : tip;

    setIcon(stateToIcon(state));
    setToolTip(tool_tip);
}

void SeafileTrayIcon::reloadTrayIcon()
{
    setIcon(stateToIcon(state_));
}

QIcon SeafileTrayIcon::getIcon(const QString& name)
{
    if (icon_cache_.contains(name)) {
        return icon_cache_[name];
    }

    QIcon icon(name);
    icon_cache_[name] = icon;
    return icon;
}

QIcon SeafileTrayIcon::stateToIcon(TrayState state)
{
    state_ = state;
#if defined(Q_OS_WIN32)
    QString icon_name;
    switch (state) {
    case STATE_DAEMON_UP:
        icon_name = utils::win::isWindows10OrHigher() ? ":/images/win/daemon_up_white.ico" : ":/images/win/daemon_up.ico";
        break;
    case STATE_DAEMON_DOWN:
        icon_name = ":/images/win/daemon_down.ico";
        break;
    case STATE_DAEMON_AUTOSYNC_DISABLED:
        icon_name = ":/images/win/seafile_auto_sync_disabled.ico";
        break;
    case STATE_TRANSFER_1:
        icon_name = ":/images/win/seafile_transfer_1.ico";
        break;
    case STATE_TRANSFER_2:
        icon_name = ":/images/win/seafile_transfer_2.ico";
        break;
    case STATE_SERVERS_NOT_CONNECTED:
        icon_name = ":/images/win/seafile_warning.ico";
        break;
    case STATE_HAVE_UNREAD_MESSAGE:
        icon_name = ":/images/win/notification.ico";
        break;
    }
    return getIcon(icon_name);
#elif defined(Q_OS_MAC)
    bool isDarkMode = utils::mac::is_darkmode();
    // filename = icon_name + ?white + .png
    QString icon_name;

    switch (state) {
    case STATE_DAEMON_UP:
        icon_name = ":/images/mac/daemon_up";
        break;
    case STATE_DAEMON_DOWN:
        icon_name = ":/images/mac/daemon_down.png";
        break;
    case STATE_DAEMON_AUTOSYNC_DISABLED:
        icon_name = ":/images/mac/seafile_auto_sync_disabled";
        break;
    case STATE_TRANSFER_1:
        icon_name = ":/images/mac/seafile_transfer_1";
        break;
    case STATE_TRANSFER_2:
        icon_name = ":/images/mac/seafile_transfer_2";
        break;
    case STATE_SERVERS_NOT_CONNECTED:
        icon_name = ":/images/mac/seafile_warning";
        break;
    case STATE_HAVE_UNREAD_MESSAGE:
        icon_name = ":/images/mac/notification";
        break;
    }
    return getIcon(icon_name + (isDarkMode ? "_white" : "") + ".png");
#else
    QString icon_name;
    switch (state) {
    case STATE_DAEMON_UP:
        icon_name = ":/images/daemon_up.png";
        break;
    case STATE_DAEMON_DOWN:
        icon_name = ":/images/daemon_down.png";
        break;
    case STATE_DAEMON_AUTOSYNC_DISABLED:
        icon_name = ":/images/seafile_auto_sync_disabled.png";
        break;
    case STATE_TRANSFER_1:
        icon_name = ":/images/seafile_transfer_1.png";
        break;
    case STATE_TRANSFER_2:
        icon_name = ":/images/seafile_transfer_2.png";
        break;
    case STATE_SERVERS_NOT_CONNECTED:
        icon_name = ":/images/seafile_warning.png";
        break;
    case STATE_HAVE_UNREAD_MESSAGE:
        icon_name = ":/images/notification.png";
        break;
    }
    return getIcon(icon_name);
#endif
}

void SeafileTrayIcon::about()
{
    QMessageBox::about(nullptr, tr("About %1").arg(getBrand()),
                       tr("<h2>Seafile Drive Client %2</h2>").arg(
                           STRINGIZE(SEADRIVE_GUI_VERSION))
#if defined(SEAFILE_CLIENT_REVISION)
                       .append("<h4> REV %1 </h4>")
                       .arg(STRINGIZE(SEAFILE_CLIENT_REVISION))
#endif
                       );
}

void SeafileTrayIcon::openHelp()
{
    QString url;
    if (QLocale::system().name() == "zh_CN") {
        url = "https://seafile.com/help/install_v2/";
    } else {
        url = "https://seafile.com/en/help/install_v2/";
    }

    QDesktopServices::openUrl(QUrl(url));
}

void SeafileTrayIcon::openSeafileFolder()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(gui->mountDir()));
}

void SeafileTrayIcon::openLogDirectory()
{
    QString log_path = QDir(gui->seadriveDir()).absoluteFilePath("logs");
    QDesktopServices::openUrl(QUrl::fromLocalFile(log_path));
}

void SeafileTrayIcon::showSettingsWindow()
{
    gui->settingsDialog()->show();
    gui->settingsDialog()->raise();
    gui->settingsDialog()->activateWindow();
}

void SeafileTrayIcon::showLoginDialog()
{
    showLoginDialog(Account());
}

void SeafileTrayIcon::showLoginDialog(const Account& account)
{
    if (!login_dlg_) {
        login_dlg_ = new LoginDialog(gui->settingsDialog());
        login_dlg_->setAttribute(Qt::WA_DeleteOnClose);
        if (!account.username.isEmpty()) {
            login_dlg_->initFromAccount(account);
        }
    }

    login_dlg_->show();
    login_dlg_->raise();
    login_dlg_->activateWindow();
    connect(login_dlg_, SIGNAL(finished(int)),
            this, SLOT(onLoginDialogClosed()));
}

void SeafileTrayIcon::showAboutDialog()
{
    gui->aboutDialog()->show();
    gui->aboutDialog()->raise();
    gui->aboutDialog()->activateWindow();
}

void SeafileTrayIcon::onActivated(QSystemTrayIcon::ActivationReason reason)
{
#if !defined(Q_OS_MAC)
    switch(reason) {
    case QSystemTrayIcon::Trigger: // single click
        openSeafileFolder();
    case QSystemTrayIcon::MiddleClick:
    case QSystemTrayIcon::DoubleClick:
        // showMainWindow();
        break;
    default:
        return;
    }
#endif
}


void SeafileTrayIcon::quitSeafile()
{
    QCoreApplication::exit(0);
}

void SeafileTrayIcon::refreshTrayIcon()
{
    if (rotate_timer_->isActive()) {
        return;
    }

    // int n_unread_msg = SeahubNotificationsMonitor::instance()->getUnreadNotifications();
    // if (n_unread_msg > 0) {
    //     setState(STATE_HAVE_UNREAD_MESSAGE,
    //              tr("You have %n message(s)", "", n_unread_msg));
    //     return;
    // }

    // if (!gui->settingsManager()->autoSync()) {
    //     setState(STATE_DAEMON_AUTOSYNC_DISABLED,
    //              tr("auto sync is disabled"));
    //     return;
    // }

    // if (!ServerStatusService::instance()->allServersConnected()) {
    //     setState(STATE_SERVERS_NOT_CONNECTED, tr("some servers not connected"));
    //     return;
    // }

    setState(STATE_DAEMON_UP);
}

void SeafileTrayIcon::refreshTrayIconToolTip()
{
    // if (!gui->settingsManager()->autoSync())
    //     return;

    // int up_rate, down_rate;
    // if (gui->rpcClient()->getUploadRate(&up_rate) < 0 ||
    //     gui->rpcClient()->getDownloadRate(&down_rate) < 0) {
    //     return;
    // }

    // if (up_rate <= 0 && down_rate <= 0) {
    //     return;
    // }

    // QString uploadStr = tr("Uploading");
    // QString downloadStr =  tr("Downloading");
    // if (up_rate > 0 && down_rate > 0) {
    //     setToolTip(QString("%1 %2/s, %3 %4/s\n").
    //                arg(uploadStr).arg(readableFileSize(up_rate)).
    //                arg(downloadStr).arg(readableFileSize(down_rate)));
    // } else if (up_rate > 0) {
    //     setToolTip(QString("%1 %2/s\n").
    //                arg(uploadStr).arg(readableFileSize(up_rate)));
    // } else /* down_rate > 0*/ {
    //     setToolTip(QString("%1 %2/s\n").
    //                arg(downloadStr).arg(readableFileSize(down_rate)));
    // }

    // rotate(true);
}


void SeafileTrayIcon::checkTrayIconMessageQueue()
{
    if (pending_messages_.empty()) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now < next_message_msec_) {
        return;
    }

    TrayMessage msg = pending_messages_.dequeue();

    // printf("[%s] tray message: %s\n",
    //        QDateTime::currentDateTime().toString().toUtf8().data(),
    //        msg.message.toUtf8().data());

#if defined(Q_OS_MAC)
    if (!utils::mac::isOSXMountainLionOrGreater()) {
        QIcon info_icon(":/images/info.png");
        TrayNotificationWidget* trayNotification = new TrayNotificationWidget(info_icon.pixmap(32, 32), msg.title, msg.message);
        tnm->append(trayNotification);
    } else {
        QSystemTrayIcon::showMessage(msg.title, msg.message, msg.icon, kMessageDisplayTimeMSecs);
    }
#else
    QSystemTrayIcon::showMessage(msg.title, msg.message, msg.icon, kMessageDisplayTimeMSecs);
#endif

    repo_id_ = msg.repo_id;
    commit_id_ = msg.commit_id;
    next_message_msec_ = now + kMessageDisplayTimeMSecs;
}

void SeafileTrayIcon::onMessageClicked()
{
    if (repo_id_.isEmpty())
        return;
    // LocalRepo repo;
    // if (seafApplet->rpcClient()->getLocalRepo(repo_id_, &repo) != 0 ||
    //     !repo.isValid() || repo.worktree_invalid)
    //     return;

    // DiffReader *reader = new DiffReader(repo, previous_commit_id_, commit_id_);
    // QThreadPool::globalInstance()->start(reader);
}

void SeafileTrayIcon::onLoginDialogClosed()
{
    login_dlg_ = nullptr;
}

// Switch to the clicked account in the account menu
void SeafileTrayIcon::onAccountItemClicked()
{
    QAction *action = (QAction *)(sender());
    Account account = qvariant_cast<Account>(action->data());

    Account current_account = gui->accountManager()->currentAccount();

    gui->accountManager()->validateAndUseAccount(account);
    if (account.isValid() && account != current_account) {
        // TODO: What if the last InitSyncDialog is not finished yet?
        InitSyncDialog *dlg = new InitSyncDialog(account);
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    }
}

void SeafileTrayIcon::logoutAccount()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    Account account = qvariant_cast<Account>(action->data());
    Q_ASSERT(account.isValid());


    QMessageBox::StandardButton ret = gui->yesNoCancelBox(
        tr("Do you want to remove the local cache?"),
        nullptr,
        QMessageBox::Cancel);

    bool remove_cache;
    if (ret == QMessageBox::Cancel) {
        return;
    } else {
        remove_cache = ret == QMessageBox::Yes;
    }

    qWarning() << "trying to log out account" << account;

    // logout Account
    LogoutDeviceRequest *req = new LogoutDeviceRequest(account, remove_cache);
    connect(req, SIGNAL(success()),
            this, SLOT(onLogoutDeviceRequestSuccess()));
    connect(req, SIGNAL(failed(const ApiError&)),
            this, SLOT(onLogoutDeviceRequestFailed(const ApiError&)));
    req->send();
}

void SeafileTrayIcon::onLogoutDeviceRequestSuccess()
{
    LogoutDeviceRequest *req = (LogoutDeviceRequest *)QObject::sender();
    const Account& account = req->account();

    if (!gui->rpcClient()->deleteAccount(account, req->shouldRemoveCache())) {
        gui->warningBox(
            tr("Failed to remove local cache of account %1").arg(account.toString()));
    }

    gui->accountManager()->clearAccountToken(account);
    req->deleteLater();
}

void SeafileTrayIcon::onLogoutDeviceRequestFailed(const ApiError& error)
{
    LogoutDeviceRequest *req = (LogoutDeviceRequest *)QObject::sender();
    req->deleteLater();
    QString msg;
    msg = tr("Failed to remove information on server: %1").arg(error.toString());
    qWarning() << "Failed to log out account" << req->account() << msg;
    gui->warningBox(msg);
}

void SeafileTrayIcon::deleteAccount()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    Account account = qvariant_cast<Account>(action->data());

    QString question = tr("Are you sure to remove account from \"%1\"?").arg(account.serverUrl.toString());

    if (!gui->yesOrNoBox(question, nullptr, false)) {
        return;
    }

    if (!gui->rpcClient()->deleteAccount(account, true)) {
        gui->warningBox(tr("Failed to delete account"));
        return;
    }

    gui->accountManager()->removeAccount(account);
}

void SeafileTrayIcon::setTransferRate(qint64 up_rate, qint64 down_rate)
{
    up_rate_ = up_rate;
    down_rate_ = down_rate;
    transfer_rate_display_action_->setText(
        tr("Up %1, Down %2")
            .arg(translateTransferRate(up_rate_),
                    translateTransferRate(down_rate_)));
}
