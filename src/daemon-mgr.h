#ifndef SEAFILE_CLIENT_DAEMON_MANAGER_H
#define SEAFILE_CLIENT_DAEMON_MANAGER_H

#include <QObject>
#include <QProcess>

class QTimer;

extern "C" {
struct _SearpcNamedPipeClient;
}

/**
 * Start/Monitor seadrive daemon
 */
class DaemonManager : public QObject {
    Q_OBJECT

public:
    DaemonManager();
    ~DaemonManager();
    void startSeadriveDaemon();

signals:
    void daemonStarted();

private slots:
    void onDaemonStarted();
    void checkDaemonReady();
    void systemShutDown();

private:
    Q_DISABLE_COPY(DaemonManager)

    QStringList collectSeaDriveArgs();
    void startSeafileDaemon();
    void stopAllDaemon();

    QTimer *conn_daemon_timer_;
    QProcess *seadrive_daemon_;

    _SearpcNamedPipeClient *searpc_pipe_client_;

    bool system_shut_down_;
};

#endif // SEAFILE_CLIENT_DAEMON_MANAGER_H
