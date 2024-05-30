#ifndef SEADRIVE_GUI_MESSAGE_POLLER_H
#define SEADRIVE_GUI_MESSAGE_POLLER_H

#include <QObject>
#include <jansson.h>

class QTimer;

class SeafileRpcClient;
class SeaDriveEvent;
class SyncCommand;

struct SyncNotification {
    QString type;
    QString repo_id;
    QString repo_name;
    QString repo_path;
    QString commit_id;
    QString parent_commit_id;
    QString commit_desc;
    QString confirmation_id;
    QString delete_files;
    QString domain_id;
    bool is_dir;
    int error_id;
    QString error;
    QString error_path;

    // cross repo move
    struct {
        QString src_path;
        QString dst_path;
        // start/done/error
        QString type;
    } move;

    bool isCrossRepoMove() const { return !move.type.isEmpty(); }

    bool isSyncError() const { return type == "sync.error"; }

    static SyncNotification fromJson(const json_t* json);
};

/**
 * Handles ccnet message
 */
class MessagePoller : public QObject {
    Q_OBJECT
public:
    MessagePoller(QObject *parent=0);
    ~MessagePoller();

    void start();
    void stop();
    void setRpcClient(SeafileRpcClient *rpc_client);
    QString lastEventType() const { return last_event_type_; }
    QString lastEventPath() const { return last_event_path_; }

signals:
    void seadriveFSLoaded();

private slots:
    void onDaemonDead();
    void onDaemonRestarted();
    void checkSeaDriveEvents();
    void checkNotification();
    void checkSyncStatus();
    void checkSyncErrors();

private:
    Q_DISABLE_COPY(MessagePoller)

    void processSeaDriveEvent(const SeaDriveEvent& event);
    void processNotification(const SyncNotification& notification);

    SeafileRpcClient *rpc_client_;
    SyncCommand *sync_command_;

    QTimer *check_notification_timer_;
    QString last_event_type_;
    QString last_event_path_;
};

#endif // SEADRIVE_GUI_MESSAGE_POLLER_H
