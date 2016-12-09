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
    void doUnmount();

signals:
    void daemonStarted();

private slots:
    void onDaemonStarted();
    void onDaemonFinished(int exit_code, QProcess::ExitStatus exit_status);
    void checkDaemonReady();
    void systemShutDown();

private:
    Q_DISABLE_COPY(DaemonManager)

    QStringList collectSeaDriveArgs();
    void startSeafileDaemon();
    void stopAllDaemon();

    QTimer *conn_daemon_timer_;
    QProcess *seadrive_daemon_;

    bool daemon_exited_;

    _SearpcNamedPipeClient *searpc_pipe_client_;

    bool system_shut_down_;
    bool unmounted_;
};

#endif // SEAFILE_CLIENT_DAEMON_MANAGER_H
