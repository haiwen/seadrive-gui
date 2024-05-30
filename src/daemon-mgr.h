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
    QString currentCacheDir() const { return current_cache_dir_; }

public slots:
    void restartSeadriveDaemon();

signals:
    void daemonStarted();

    void daemonDead();
    void daemonRestarted();

private slots:
    void onDaemonStarted();
    void onDaemonFinished(int exit_code, QProcess::ExitStatus exit_status);
    void checkDaemonReady();
    void seadriveExiting();

private:
    Q_DISABLE_COPY(DaemonManager)

    QStringList collectSeaDriveArgs();
    void startSeafileDaemon();
    void stopAllDaemon();
    void scheduleRestartDaemon();
    void transitionState(int new_state);

    QTimer *conn_daemon_timer_;
    QProcess *seadrive_daemon_;

    int current_state_;
    // Used to decide whether to emit daemonStarted or daemonRestarted
    bool first_start_;
    int restart_retried_;
    _SearpcNamedPipeClient *searpc_pipe_client_;
    QString current_cache_dir_;

};

#endif // SEAFILE_CLIENT_DAEMON_MANAGER_H
