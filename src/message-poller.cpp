#include <QTimer>

#include "utils/translate-commit-desc.h"
#include "utils/json-utils.h"
#include "seadrive-gui.h"
#include "rpc/rpc-client.h"
#include "ui/tray-icon.h"

#include "message-poller.h"

namespace {

const int kCheckNotificationIntervalMSecs = 1000;

} // namespace


MessagePoller::MessagePoller(QObject *parent): QObject(parent)
{
    rpc_client_ = new SeafileRpcClient();
    check_notification_timer_ = new QTimer(this);
    connect(check_notification_timer_, SIGNAL(timeout()), this, SLOT(checkNotification()));
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
    printf ("check notification called\n");
    json_t *ret;
    if (!rpc_client_->getSyncNotification(&ret)) {
        return;
    }
    SyncNotification notification = SyncNotification::fromJson(ret);
    processNotification(notification);
}

void MessagePoller::processNotification(const SyncNotification& notification)
{
    printf("notification.repo_name = %s\n", notification.repo_name.toUtf8().data());
    printf("notification.type = %s\n", notification.type.toUtf8().data());
    if (notification.type == "sync.done") {
        QString title = tr("\"%1\" is synchronized").arg(notification.repo_name);
        gui->trayIcon()->showMessage(
            title,
            translateCommitDesc(notification.commit_desc),
            notification.repo_id,
            notification.commit_id,
            notification.parent_commit_id);
    }
}

SyncNotification SyncNotification::fromJson(const json_t *root)
{
    SyncNotification notification;
    Json json(root);

    notification.type = json.getString("type");
    notification.repo_id = json.getString("repo_id");
    notification.repo_name = json.getString("repo_name");
    notification.commit_id = json.getString("commit_id");
    notification.parent_commit_id = json.getString("parent_commit_id");
    notification.commit_desc = json.getString("commit_desc");

    return notification;
}
