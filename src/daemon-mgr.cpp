extern "C" {
#include <searpc-client.h>
#include <searpc-named-pipe-transport.h>
}

#include <unistd.h>
#include <glib-object.h>
#include <cstdio>
#include <cstdlib>
#include <QTimer>
#include <QStringList>
#include <QString>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>

#include "utils/utils.h"
#include "utils/process.h"
#include "seadrive-gui.h"
#include "daemon-mgr.h"

namespace {

const int kConnDaemonIntervalMilli = 1000;
const char *kSeadriveSockName = "seadrive.sock";

#if defined(Q_OS_WIN32)
const char *kSeadriveExecutable = "seadrive.exe";
#else
const char *kSeadriveExecutable = "seadrive";
#endif

} // namespace


DaemonManager::DaemonManager()
    : seadrive_daemon_(nullptr),
      searpc_pipe_client_(nullptr)
{
    conn_daemon_timer_ = new QTimer(this);
    connect(conn_daemon_timer_, SIGNAL(timeout()), this, SLOT(checkDaemonReady()));
    shutdown_process (kSeadriveExecutable);

    system_shut_down_ = false;
    connect(qApp, SIGNAL(aboutToQuit()),
            this, SLOT(systemShutDown()));
}

DaemonManager::~DaemonManager() {
    stopAllDaemon();
}

void DaemonManager::startSeadriveDaemon()
{
    QDir data_dir(gui->seadriveDataDir());

    searpc_pipe_client_ = searpc_create_named_pipe_client(toCStr(data_dir.filePath(kSeadriveSockName)));

    seadrive_daemon_ = new QProcess(this);
    connect(seadrive_daemon_, SIGNAL(started()), this, SLOT(onDaemonStarted()));

    QStringList args;
    args << "-f";
    args << "-d" << data_dir.absolutePath();
    args << "-l" << QDir(gui->logsDir()).absoluteFilePath("seadrive.log");

    QString fuse_opts = qgetenv("SEADRIVE_FUSE_OPTS");
    if (fuse_opts.isEmpty()) {
#if defined(Q_OS_MAC)
        QProcess::execute("umount", QStringList(gui->mountDir()));
        fuse_opts = gui->mountDir();
        fuse_opts += " -o volname=SeaDrive,noappledouble";
#elif defined(Q_OS_LINUX)
        QStringList umount_arguments;
        umount_arguments << "-u" << gui->mountDir();
        QProcess::execute("fusermount", umount_arguments);
        fuse_opts = gui->mountDir();
#endif
    }
    args << fuse_opts.split(" ");
    auto stream = qInfo() << "starting seadrive daemon:" << kSeadriveExecutable;
    foreach (const QString& arg, args) {
        stream << arg;
    }

    seadrive_daemon_->start(RESOURCE_PATH(kSeadriveExecutable), args);
}

void DaemonManager::systemShutDown()
{
    system_shut_down_ = true;
}

void DaemonManager::onDaemonStarted()
{
    qDebug("seadrive daemon is now running, checking if the service is ready");
    conn_daemon_timer_->start(kConnDaemonIntervalMilli);
}

void DaemonManager::checkDaemonReady()
{
    QString str;
    if (searpc_named_pipe_client_connect(searpc_pipe_client_) == 0) {
        // TODO: Instead of only connecting to the rpc server, we should make a
        // real rpc call here so we can guarantee the daemon is ready to answer
        // rpc requests.
        qDebug("seadrive daemon is ready");
        conn_daemon_timer_->stop();
        emit daemonStarted();
        // TODO: Free the searpc client.
        // ::close(searpc_pipe_client_->pipe_fd);
        return;
    }
    qDebug("seadrive daemon is not ready");
    static int maxcheck = 0;
    if (++maxcheck > 15) {
        qWarning("seadrive rpc is not ready after %d retry, abort", maxcheck);
        gui->errorAndExit(tr("%1 drive failed to initialize").arg(getBrand()));
    }
}

void DaemonManager::stopAllDaemon()
{
    qWarning("[Daemon Mgr] stopping seadrive daemon");

    if (conn_daemon_timer_)
        conn_daemon_timer_->stop();
    if (seadrive_daemon_) {
        seadrive_daemon_->kill();
        seadrive_daemon_->waitForFinished(50);
    }
}
