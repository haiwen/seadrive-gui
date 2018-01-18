#include <QTimer>
#include <QDateTime>

#include "utils/utils.h"
#include "utils/translate-commit-desc.h"
#include "utils/json-utils.h"
#include "utils/file-utils.h"
#include "seadrive-gui.h"
#include "daemon-mgr.h"
#include "settings-mgr.h"
#include "rpc/rpc-client.h"
#include "rpc/sync-error.h"
#include "ui/tray-icon.h"

#include "message-poller.h"

namespace {

const int kCheckNotificationIntervalMSecs = 1000;

struct GlobalSyncStatus {
    bool is_syncing;
    qint64 sent_bytes;
    qint64 recv_bytes;

    static GlobalSyncStatus fromJson(const json_t* json);
};

} // namespace

class SeaDriveEvent {
public:
    enum FsOpError {
        UNKNOWN_ERROR = 0,
        CREATE_ROOT_FILE,
        REMOVE_REPO,
    };

    FsOpError fs_op_error;
    QString path, type;

    static SeaDriveEvent fromJson(json_t * root) {
        // char *s = json_dumps(root, 0);
        // printf ("[%s] %s\n", QDateTime::currentDateTime().toString().toUtf8().data(), s);
        // free (s);

        SeaDriveEvent event;
        Json json(root);

        QString type = json.getString("type");
        if (type == "fs_op_error.create_root_file") {
            event.fs_op_error = CREATE_ROOT_FILE;
        } else if (type == "fs_op_error.remove_repo") {
            event.fs_op_error = REMOVE_REPO;
        } else {
            qWarning("unknown type of seadrive event %s", toCStr(type));
            event.fs_op_error = UNKNOWN_ERROR;
        }
        event.path = json.getString("path");
        event.type = type;

        return event;
    }
};


MessagePoller::MessagePoller(QObject *parent): QObject(parent)
{
    rpc_client_ = new SeafileRpcClient();
    check_notification_timer_ = new QTimer(this);
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkSeaDriveEvents()));
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkNotification()));
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkSyncStatus()));
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkSyncErrors()));
}

MessagePoller::~MessagePoller()
{
}

void MessagePoller::start()
{
    rpc_client_->connectDaemon();
    check_notification_timer_->start(kCheckNotificationIntervalMSecs);
    connect(gui->daemonManager(), SIGNAL(daemonDead()), this, SLOT(onDaemonDead()));
    connect(gui->daemonManager(), SIGNAL(daemonRestarted()), this, SLOT(onDaemonRestarted()));
}

void MessagePoller::onDaemonDead()
{
    qDebug("pausing message poller when daemon is dead");
    check_notification_timer_->stop();
}

void MessagePoller::onDaemonRestarted()
{
    qDebug("reviving message poller when daemon is restarted");
    if (rpc_client_) {
        delete rpc_client_;
    }
    rpc_client_ = new SeafileRpcClient();
    rpc_client_->connectDaemon();
    check_notification_timer_->start(kCheckNotificationIntervalMSecs);
}

void MessagePoller::checkSeaDriveEvents()
{
    json_t *ret;
    if (!rpc_client_->getSeaDriveEvents(&ret)) {
        return;
    }
    SeaDriveEvent event = SeaDriveEvent::fromJson(ret);
    json_decref(ret);

    processSeaDriveEvent(event);
}

void MessagePoller::checkNotification()
{
    json_t *ret;
    if (!rpc_client_->getSyncNotification(&ret)) {
        return;
    }
    SyncNotification notification = SyncNotification::fromJson(ret);
    json_decref(ret);

    processNotification(notification);
}

void MessagePoller::checkSyncStatus()
{
    json_t *ret;
    if (!rpc_client_->getGlobalSyncStatus(&ret)) {
        return;
    }
    GlobalSyncStatus sync_status = GlobalSyncStatus::fromJson(ret);
    json_decref(ret);

    if (sync_status.is_syncing) {
        gui->trayIcon()->rotate(true);
        gui->trayIcon()->setTransferRate(sync_status.sent_bytes, sync_status.recv_bytes);
    } else {
        gui->trayIcon()->rotate(false);
        gui->trayIcon()->setTransferRate(0, 0);
    }
}

void MessagePoller::checkSyncErrors()
{
    json_t *ret;
    if (!rpc_client_->getSyncErrors(&ret)) {
        gui->trayIcon()->setSyncErrors(QList<SyncError>());
        return;
    }

    QList<SyncError> errors = SyncError::listFromJSON(ret);
    json_decref(ret);

    gui->trayIcon()->setSyncErrors(errors);
}

void MessagePoller::processNotification(const SyncNotification& notification)
{
    if (notification.type == "sync.done") {
        if (!gui->settingsManager()->notify()) {
            return;
        }
        QString title = tr("\"%1\" is synchronized").arg(notification.repo_name);
        gui->trayIcon()->showMessage(
            title,
            translateCommitDesc(notification.commit_desc),
            notification.repo_id,
            notification.commit_id,
            notification.parent_commit_id);
    } else if (notification.type == "sync.error") {
        QString path_in_title;
        if (!notification.repo_name.isEmpty()) {
            path_in_title = notification.repo_name;
        } else if (!notification.error_path.isEmpty()) {
            path_in_title = ::getBaseName(notification.error_path);
        }
        QString title;
        if (!path_in_title.isEmpty()) {
            title = tr("Error when syncing \"%1\"").arg(path_in_title);
        } else {
            title = tr("Error when syncing");
        }
        gui->trayIcon()->showMessage(
            title,
            notification.error,
            notification.repo_id,
            "",
            "",
            QSystemTrayIcon::Warning);
    } else if (notification.type == "sync.multipart_upload") {
        if (!gui->settingsManager()->notify()) {
            return;
        }
        QString title = tr("\"%1\" is being uploaded").arg(notification.repo_name);
        gui->trayIcon()->showMessage(
            title,
            translateCommitDesc(notification.commit_desc),
            notification.repo_id,
            notification.commit_id,
            notification.parent_commit_id);
    } else if (notification.type == "fs-loaded") {
        emit seadriveFSLoaded();
    } else if (notification.isCrossRepoMove()) {
        // printf("src path = %s, dst path = %s\n", toCStr(notification.move.src_path), toCStr(notification.move.dst_path));
        QString src = ::getBaseName(notification.move.src_path);
        QString dst = ::getParentPath(notification.move.dst_path) + "/";
        QString title, msg;

        if (notification.move.type == "start") {
            title = tr("Starting to move \"%1\"").arg(src);
            msg = tr("Starting to move \"%1\" to \"%2\"").arg(src, dst);
        } else if (notification.move.type == "done") {
            title = tr("Successfully moved \"%1\"").arg(src);
            msg = tr("Successfully moved \"%1\" to \"%2\"").arg(src, dst);
        } else if (notification.move.type == "error") {
            title = tr("Failed to move \"%1\"").arg(src);
            msg = tr("Failed to move \"%1\" to \"%2\"").arg(src, dst);
        }

        gui->trayIcon()->showMessage(
            title,
            msg,
            "",
            "",
            "",
            QSystemTrayIcon::Information);
    } else {
        qWarning ("Unknown message %s\n", notification.type.toUtf8().data());
    }
}

void MessagePoller::processSeaDriveEvent(const SeaDriveEvent &event)
{
    last_event_path_ = event.path;
    if(event.type == "file-download.start") {
        QString title = tr("Download file");
        QString msg = tr("Start to download file \"%1\" ").arg(::getBaseName(event.path));
        gui->trayIcon()->showMessage(title, msg);
        last_event_type_ = event.type;
        return;
    } else if (event.type == "file-download.done") {
        QString title = tr("Download file");
        QString msg = tr("file \"%1\" has been downloaded ").arg(::getBaseName(event.path));
        gui->trayIcon()->showMessage(title, msg);
        last_event_type_ = event.type;
        return;
    }

    switch (event.fs_op_error) {
        case SeaDriveEvent::CREATE_ROOT_FILE: {
            QString title = tr("Failed to create file \"%1\"").arg(::getBaseName(event.path));
            QString msg =
                tr("You can't create files in the %1 drive directly")
                    .arg(gui->mountDir());
            gui->trayIcon()->showWarningMessage(title, msg);
        } break;
        case SeaDriveEvent::REMOVE_REPO: {
            QString title = tr("Failed to delete folder");
            QString msg = tr("You can't delete the library \"%1\" directly").arg(::getBaseName(event.path));
            gui->trayIcon()->showWarningMessage(title, msg);
        } break;
    default:
        break;
    }
}

SyncNotification SyncNotification::fromJson(const json_t *root)
{
    SyncNotification notification;
    Json json(root);

    // char *s = json_dumps(root, 0);
    // printf ("[%s] %s\n", QDateTime::currentDateTime().toString().toUtf8().data(), s);
    // qWarning ("[%s] %s\n", QDateTime::currentDateTime().toString().toUtf8().data(), s);
    // free (s);

    notification.type = json.getString("type");

    if (notification.type.startsWith("cross-repo-move.")) {
        notification.move.src_path = json.getString("srcpath");
        notification.move.dst_path = json.getString("dstpath");
        notification.move.type = notification.type.split(".").last();
    } else {
        notification.repo_id = json.getString("repo_id");
        notification.repo_name = json.getString("repo_name");
        notification.commit_id = json.getString("commit_id");
        notification.parent_commit_id = json.getString("parent_commit_id");
        notification.commit_desc = json.getString("commit_desc");
        if (notification.isSyncError()) {
            notification.error_id = json.getLong("err_id");
            notification.error_path = json.getString("path");
            notification.error = SyncError::syncErrorIdToErrorStr(notification.error_id, notification.error_path);
        }
    }

    return notification;
}


GlobalSyncStatus GlobalSyncStatus::fromJson(const json_t *root)
{
    GlobalSyncStatus sync_status;
    Json json(root);

    sync_status.is_syncing = json.getLong("is_syncing");
    sync_status.sent_bytes = json.getLong("sent_bytes");
    sync_status.recv_bytes = json.getLong("recv_bytes");

    // char *s = json_dumps(root, 0);
    // printf ("[%s] %s\n", QDateTime::currentDateTime().toString().toUtf8().data(), s);
    // free (s);

    return sync_status;
}
