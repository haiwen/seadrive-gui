#include <QTimer>
#include <QDateTime>

#include "utils/utils.h"
#include "utils/translate-commit-desc.h"
#include "utils/json-utils.h"
#include "utils/file-utils.h"
#include "seadrive-gui.h"
#include "settings-mgr.h"
#include "rpc/rpc-client.h"
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


// Copied from seadrive/src/sync-mgr.c
#define SYNC_ERROR_ID_FILE_LOCKED_BY_APP 0
#define SYNC_ERROR_ID_FOLDER_LOCKED_BY_APP 1
#define SYNC_ERROR_ID_FILE_LOCKED 2
#define SYNC_ERROR_ID_INVALID_PATH 3
#define SYNC_ERROR_ID_INDEX_ERROR 4
#define SYNC_ERROR_ID_ACCESS_DENIED 5
#define SYNC_ERROR_ID_QUOTA_FULL 6


QString translateNotificationError(SyncNotification notification)
{
    bool has_path = !notification.error_path.isEmpty();
    QString file;
    if (has_path) {
        file = ::getBaseName(notification.error_path);
    }
    switch (notification.error_id) {
    case SYNC_ERROR_ID_FILE_LOCKED_BY_APP: {
        if (has_path) {
            return QObject::tr("File %1 is locked by other programs").arg(file);
        } else {
            return QObject::tr("Some file is locked by other programs");
        }
        break;
    }
    case SYNC_ERROR_ID_FOLDER_LOCKED_BY_APP: {
        if (has_path) {
            return QObject::tr("Folder %1 is locked by other programs").arg(file);
        } else {
            return QObject::tr("Some folder is locked by other programs");
        }
        break;
    }
    case SYNC_ERROR_ID_FILE_LOCKED: {
        if (has_path) {
            return QObject::tr("File %1 is locked by another user").arg(file);
        } else {
            return QObject::tr("Some file is locked by another user");
        }
        break;
    }
    case SYNC_ERROR_ID_INVALID_PATH: {
        if (has_path) {
            return QObject::tr("Invalid path %1").arg(file);
        } else {
            return QObject::tr("Trying to access an invalid path");
        }
        break;
    }
    case SYNC_ERROR_ID_INDEX_ERROR: {
        if (has_path) {
            return QObject::tr("Error when indexing file %1").arg(file);
        } else {
            return QObject::tr("Error when indexing files");
        }
        break;
    }
    case SYNC_ERROR_ID_ACCESS_DENIED: {
        return QObject::tr("You don't have enough permission for this library");
        break;
    }
    case SYNC_ERROR_ID_QUOTA_FULL: {
        return QObject::tr("The storage quota has been used up");
        break;
    }
    default:
        return QObject::tr("Unknown error");
    }
}

} // namespace


MessagePoller::MessagePoller(QObject *parent): QObject(parent)
{
    rpc_client_ = new SeafileRpcClient();
    check_notification_timer_ = new QTimer(this);
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkNotification()));
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkSyncStatus()));
}

MessagePoller::~MessagePoller()
{
}

void MessagePoller::start()
{
    rpc_client_->connectDaemon();
    check_notification_timer_->start(kCheckNotificationIntervalMSecs);
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
        QString title = tr("Error when syncing \"%1\"").arg(notification.repo_name);
        gui->trayIcon()->showMessage(
            title,
            notification.error,
            notification.repo_id,
            "",
            "",
            QSystemTrayIcon::Warning);
    } else if (notification.type == "fs-loaded") {
        emit seadriveFSLoaded();
    } else if (notification.isCrossRepoMove()) {
        // printf("src path = %s, dst path = %s\n", toCStr(notification.move.src_path), toCStr(notification.move.dst_path));
        QString src = ::getBaseName(notification.move.src_path);
        QString dst = ::getParentPath(notification.move.dst_path);
        QString title, msg;

        if (notification.move.type == "start") {
            title = tr("Starting to move %1").arg(src);
            msg = tr("Starting to move %1 to %2/").arg(src, dst);
        } else if (notification.move.type == "done") {
            title = tr("Successfully moved %1").arg(src);
            msg = tr("Successfully moved %1 to %2/").arg(src, dst);
        } else if (notification.move.type == "error") {
            title = tr("Failed to move %1").arg(src);
            msg = tr("Failed to moved %1 to %2/").arg(src, dst);
        }

        gui->trayIcon()->showMessage(
            title,
            msg,
            "",
            "",
            "",
            QSystemTrayIcon::Warning);
    } else {
        printf ("Unknown message %s\n", notification.type.toUtf8().data());
    }
}

SyncNotification SyncNotification::fromJson(const json_t *root)
{
    SyncNotification notification;
    Json json(root);

    // char *s = json_dumps(root, 0);
    // printf ("[%s] %s\n", QDateTime::currentDateTime().toString().toUtf8().data(), s);
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
            notification.error = translateNotificationError(notification);
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
