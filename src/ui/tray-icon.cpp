#include <glib.h>

#include <QtGlobal>

#include <QtWidgets>
#include <QApplication>
#include <QDesktopServices>
#include <QSet>
#include <QDebug>
#include <QMenuBar>
#include <QRunnable>

#include "message-poller.h"
#include "utils/utils.h"
#include "utils/utils-mac.h"
#include "utils/file-utils.h"
#include "src/ui/settings-dialog.h"
#include "src/ui/login-dialog.h"
#include "src/ui/init-sync-dialog.h"
#include "src/ui/about-dialog.h"
#include "src/ui/encrypted-repos-dialog.h"
#include "src/ui/sync-errors-dialog.h"
#include "src/ui/transfer-progress-dialog.h"
#include "api/api-error.h"
#include "api/requests.h"
#include "seadrive-gui.h"
#include "account-mgr.h"
#include "rpc/rpc-client.h"
#include "file-provider-mgr.h"

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
#if defined (Q_OS_WIN32)
const char* const kPreconfigureUseKerberosLogin = "PreconfigureUseKerberosLogin";
#endif

} // namespace

SeafileTrayIcon::SeafileTrayIcon(QObject *parent)
    : QSystemTrayIcon(parent),
      nth_trayicon_(0),
      rotate_counter_(0),
      state_(STATE_NONE),
      next_message_msec_(0),
      login_dlg_(nullptr),
      up_rate_(0),
      down_rate_(0),
      sync_errors_dialog_(nullptr),
      transfer_progress_dialog_(nullptr),
      enc_repo_dialog_(nullptr),
      enable_login_action_(true)
{
    setState(STATE_DAEMON_DOWN);
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
    transfer_rate_display_action_->setEnabled(true);
    transfer_progress_action_->setEnabled(true);
    global_sync_error_action_->setEnabled(true);
    show_sync_errors_action_->setEnabled(true);
    show_enc_repos_action_->setEnabled(true);

    setState(STATE_DAEMON_UP);

    refresh_timer_->start(kRefreshInterval);
}

void SeafileTrayIcon::createActions()
{
    // The text would be set at the menu open time.
    transfer_rate_display_action_ = new QAction(tr("Starting ..."), this);

    transfer_progress_action_ = new QAction(tr("Transfer progress"), this);
    connect(transfer_progress_action_, SIGNAL(triggered()), this, SLOT(showTransferProgressDialog()));

    show_enc_repos_action_ = new QAction(tr("Show encrypted libraries"), this);
    connect(show_enc_repos_action_, SIGNAL(triggered()), this, SLOT(showEncRepoDialog()));

    quit_action_ = new QAction(tr("&Quit"), this);
    connect(quit_action_, SIGNAL(triggered()), this, SLOT(quitSeafile()));


    settings_action_ = new QAction(tr("Settings"), this);
    connect(settings_action_, SIGNAL(triggered()), this, SLOT(showSettingsWindow()));

    show_sync_errors_action_ = new QAction(tr("Show file sync errors"), this);
    connect(show_sync_errors_action_, SIGNAL(triggered()), this, SLOT(showSyncErrorsDialog()));

    global_sync_error_action_ = new QAction("", this);

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
    transfer_rate_display_action_->setEnabled(false);
    transfer_progress_action_->setEnabled(false);
    global_sync_error_action_->setEnabled(false);
    show_sync_errors_action_->setEnabled(false);
    show_enc_repos_action_->setEnabled(false);

    context_menu_ = new QMenu(NULL);
    context_menu_->addAction(transfer_rate_display_action_);
    context_menu_->addAction(transfer_progress_action_);
    context_menu_->addAction(global_sync_error_action_);
    context_menu_->addAction(show_sync_errors_action_);
    context_menu_->addSeparator();

    context_menu_->addAction(show_enc_repos_action_);
    context_menu_->addSeparator();

    context_menu_->addAction(open_log_directory_action_);
    context_menu_->addAction(settings_action_);

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
    auto accounts = gui->accountManager()->allAccounts();

    if (global_sync_error_.isValid()) {
        global_sync_error_action_->setVisible(true);
        global_sync_error_action_->setText(global_sync_error_.error_str);
    } else {
        global_sync_error_action_->setVisible(false);
    }

    show_sync_errors_action_->setVisible(true);

    // Remove all menu items and recreate them.
    account_menu_->clear();

    if (!accounts.empty()) {
        for (size_t i = 0, n = accounts.size(); i < n; i++) {
            const Account &account = accounts[i];
            QString text_name = account.accountInfo.name.isEmpty() ?
                        account.username : account.accountInfo.name;
            QString text = text_name + " (" + account.serverUrl.host() + ")";
            if (!account.isValid()) {
                text += ", " + tr("not logged in");
            }
            QMenu *submenu = new QMenu(text, account_menu_);
            if (account.isValid()) {
                submenu->setIcon(QIcon(":/images/account-checked.png"));
            } else {
                submenu->setIcon(QIcon(":/images/account-else.png"));
            }

            QAction *delete_account_action = new QAction(tr("Delete"), this);
            delete_account_action->setIcon(QIcon(":/images/delete-account.png"));
            delete_account_action->setIconVisibleInMenu(true);
            delete_account_action->setData(QVariant::fromValue(account));
            connect(delete_account_action, SIGNAL(triggered()), this, SLOT(deleteAccount()));
            submenu->addAction(delete_account_action);

            QAction *resync_account_action = new QAction(tr("Resync"), this);
            resync_account_action->setIcon(QIcon(":/images/resync.png"));
            resync_account_action->setIconVisibleInMenu(true);
            resync_account_action->setData(QVariant::fromValue(account));
            connect(resync_account_action, SIGNAL(triggered()), this, SLOT(resyncAccount()));
            submenu->addAction(resync_account_action);

            account_menu_->addMenu(submenu);
        }

        account_menu_->addSeparator();
    }

    login_action_ = new QAction(tr("Add an account"), this);
    login_action_->setIcon(QIcon(":/images/add-account.png"));
    login_action_->setIconVisibleInMenu(true);
    login_action_->setEnabled(enable_login_action_);
    connect(login_action_, SIGNAL(triggered()), this, SLOT(showLoginDialog()));
    account_menu_->addAction(login_action_);
#if defined(Q_OS_WIN32)
    QVariant use_kerberos_login = gui->readPreconfigureExpandedString(kPreconfigureUseKerberosLogin, "0");
    bool is_use_kerberos_login = use_kerberos_login.toBool();
    if (is_use_kerberos_login) {
        account_menu_->removeAction(login_action_);
    }
#endif
}

void SeafileTrayIcon::createGlobalMenuBar()
{
    // support it only on mac os x currently
    // TODO: destroy the objects when seafile closes
#ifdef Q_OS_MAC
    // create qmenu used in menubar and docker menu
    global_menu_ = new QMenu(tr("File"));
    global_menu_->addAction(open_log_directory_action_);
    global_menu_->addSeparator();

    global_menubar_ = new QMenuBar(0);
    global_menubar_->addMenu(global_menu_);
    // TODO fix the line below which crashes under qt5.4.0
    //global_menubar_->addMenu(help_menu_);
    global_menubar_->setNativeMenuBar(true);
    qApp->setAttribute(Qt::AA_DontUseNativeMenuBar, false);

    global_menu_->setAsDockMenu(); // available after qt5.2.0
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

void SeafileTrayIcon::showWarningMessage(const QString &title,
                                         const QString &message)
{
    showMessage(title, message, "", "", "", QSystemTrayIcon::Warning);
}

void SeafileTrayIcon::showMessage(const QString &title,
                                  const QString &message,
                                  const QString &repo_id,
                                  const QString &commit_id,
                                  const QString &previous_commit_id,
                                  MessageIcon icon)
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
        setStateWithSyncErrors();
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
#ifdef Q_OS_MAC
    // The icon style has been changed to monochrome on macOS.
    icon.setIsMask(true);
#endif
    icon_cache_[name] = icon;
    return icon;
}

QIcon SeafileTrayIcon::stateToIcon(TrayState state)
{
    state_ = state;
#if defined(Q_OS_WIN32)
    QString icon_name;
    bool use_white = utils::win::isWindows10OrHigher();
    switch (state) {
    case STATE_NONE:
    case STATE_DAEMON_UP:
        icon_name = "daemon_up";
        break;
    case STATE_DAEMON_DOWN:
        icon_name = "daemon_down";
        break;
    case STATE_DAEMON_AUTOSYNC_DISABLED:
        icon_name = "seafile_auto_sync_disabled";
        break;
    case STATE_TRANSFER_1:
        use_white = false;
        icon_name = "seafile_transfer_1";
        break;
    case STATE_TRANSFER_2:
        use_white = false;
        icon_name = "seafile_transfer_2";
        break;
    case STATE_SERVERS_NOT_CONNECTED:
    case STATE_HAS_SYNC_ERRORS:
        icon_name = "seafile_warning";
        break;
    case STATE_HAVE_UNREAD_MESSAGE:
        icon_name = "notification";
        break;
    }
    if (use_white) {
        icon_name += "_white";
    }
    QString full_icon_name = QString(":/images/win/%1.ico").arg(icon_name);
    return getIcon(full_icon_name);
#elif defined(Q_OS_MAC)
    QString icon_name;

    switch (state) {
    case STATE_NONE:
    case STATE_DAEMON_UP:
        icon_name = ":/images/mac/daemon_up";
        break;
    case STATE_DAEMON_DOWN:
        icon_name = ":/images/mac/daemon_down";
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
    case STATE_HAS_SYNC_ERRORS:
        icon_name = ":/images/mac/seafile_warning";
        break;
    case STATE_HAVE_UNREAD_MESSAGE:
        icon_name = ":/images/mac/notification";
        break;
    }
    return getIcon(icon_name + ".png");
#else
    QString icon_name;
    switch (state) {
    case STATE_NONE:
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
    case STATE_HAS_SYNC_ERRORS:
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
        url = "https://cloud.seafile.com/published/seafile-user-manual/seadrive_client/install_seadrive_client.md";
    } else {
        url = "https://download.seafile.com/published/seafile-user-manual/drive_client/using_drive_client.md";
    }

    QDesktopServices::openUrl(QUrl(url));
}

void SeafileTrayIcon::openLogDirectory()
{
    QString log_dir = QFileInfo(seadriveLogDir()).absoluteFilePath();
    QDesktopServices::openUrl(QUrl::fromLocalFile(log_dir));
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
        QDesktopServices::openUrl(QUrl::fromLocalFile(gui->seadriveRoot()));
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

    setStateWithSyncErrors();
}

void SeafileTrayIcon::refreshTrayIconToolTip()
{
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
        QSystemTrayIcon::showMessage(msg.title, msg.message, msg.icon, 0);
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
    //if (repo_id_.isEmpty())
    //    return;
    // LocalRepo repo;
    // if (seafApplet->rpcClient()->getLocalRepo(repo_id_, &repo) != 0 ||
    //     !repo.isValid() || repo.worktree_invalid)
    //     return;

    // DiffReader *reader = new DiffReader(repo, previous_commit_id_, commit_id_);
    // QThreadPool::globalInstance()->start(reader);
    if (gui->messagePoller()->lastEventType() == "file-download.start") {
        showTransferProgressDialog();
        transfer_progress_dialog_->showDownloadTab();
    }
}

void SeafileTrayIcon::onLoginDialogClosed()
{
    login_dlg_ = nullptr;
}

void SeafileTrayIcon::deleteAccount()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    Account account = qvariant_cast<Account>(action->data());

    bool is_uploading = gui->rpcClient()->isAccountUploading (account);
    if (is_uploading) {
        gui->warningBox (tr("There are changes being uploaded under the account, please try again later"));
        return;
    }

#ifdef Q_OS_WIN32
    QString question = tr("Are you sure to remove account from \"%1\"?").arg(account.serverUrl.toString());
#else
    QString question = tr("Are you sure to remove account from \"%1\"? After removing account, you can still find downloaded files at ~/Library/CloudStorage.").arg(account.serverUrl.toString());
#endif

    if (!gui->yesOrNoBox(question, nullptr, false)) {
        return;
    }

    gui->accountManager()->removeAccount(account);
}

void SeafileTrayIcon::resyncAccount()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (!action)
        return;
    Account account = qvariant_cast<Account>(action->data());

    bool is_uploading = gui->rpcClient()->isAccountUploading (account);
    if (is_uploading) {
        gui->warningBox (tr("There are changes being uploaded under the account, please try again later"));
        return;
    }

    QString question = tr("The account will be synced to a new sync root folder. Are you sure to resync account from \"%1\"?").arg(account.serverUrl.toString());

    if (!gui->yesOrNoBox(question, nullptr, false)) {
        return;
    }

    gui->accountManager()->resyncAccount(account);
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

void SeafileTrayIcon::setSyncErrors(const QList<SyncError> errors)
{
    sync_errors_.clear();
    global_sync_error_ = SyncError();

    foreach (const SyncError& error, errors) {
        if (error.isGlobalError()) {
            if (global_sync_error_.timestamp < error.timestamp) {
                global_sync_error_ = error;
            }
        } else {
            sync_errors_.push_back(error);
        }
    }
    reloadTrayIcon();
}

void SeafileTrayIcon::setStateWithSyncErrors()
{
    qint64 timestamp;
    if(sync_errors_dialog_ != nullptr) {
        timestamp = sync_errors_dialog_->getLastOpenSyncDialogTimestamp();
    } else {
        timestamp = 0;
    }
    if (global_sync_error_.isValid()) {
        setState(STATE_HAS_SYNC_ERRORS);
    } else if(!sync_errors_.isEmpty()) {
        if(timestamp > sync_errors_[0].timestamp) {
            setState(STATE_DAEMON_UP);
        } else {
            setState(STATE_HAS_SYNC_ERRORS);
        }
    } else {
        setState(STATE_DAEMON_UP);
    }
}

void SeafileTrayIcon::setLoginActionEnabled(bool enabled)
{
    enable_login_action_ = enabled;
}

void SeafileTrayIcon::showSyncErrorsDialog()
{
    gui->refreshQss();
    if (sync_errors_dialog_ == nullptr) {
        sync_errors_dialog_ = new SyncErrorsDialog;
    }

    sync_errors_dialog_->updateErrors();
    sync_errors_dialog_->show();
    sync_errors_dialog_->raise();
    sync_errors_dialog_->activateWindow();
}

void SeafileTrayIcon::showTransferProgressDialog()
{
    if (transfer_progress_dialog_ == nullptr) {
        transfer_progress_dialog_ = new TransferProgressDialog();
    }

    // A bug that changes default button styles is fixed here by
    // delaying the dialog 10ms.
    QTimer::singleShot(10, this, [&] {
        transfer_progress_dialog_->show();
        transfer_progress_dialog_->raise();
        transfer_progress_dialog_->activateWindow();
    });
}

void SeafileTrayIcon::showEncRepoDialog() {

    if (enc_repo_dialog_ == nullptr) {
        enc_repo_dialog_ = new EncryptedReposDialog();
    }

    enc_repo_dialog_->show();
    enc_repo_dialog_->raise();
    enc_repo_dialog_->activateWindow();
}
