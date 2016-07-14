#ifndef SEADRIVE_GUI_MESSAGE_POLLER_H
#define SEADRIVE_GUI_MESSAGE_POLLER_H

#include <QObject>
#include <jansson.h>

class QTimer;

class SeafileRpcClient;

struct SyncNotification {
    QString type;
    QString repo_id;
    QString repo_name;
    QString commit_id;
    QString parent_commit_id;
    QString commit_desc;

    static SyncNotification fromJson(const json_t* json);
};

/**
 * Handles ccnet message
 */
class MessagePoller : QObject {
    Q_OBJECT
public:
    MessagePoller(QObject *parent=0);
    ~MessagePoller();

    void start();

private slots:
    void checkNotification();

private:
    Q_DISABLE_COPY(MessagePoller)

    void processNotification(const SyncNotification& notification);

    SeafileRpcClient *rpc_client_;

    QTimer *check_notification_timer_;
};

#endif // SEADRIVE_GUI_MESSAGE_POLLER_H
