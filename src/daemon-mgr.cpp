#include <searpc-client.h>
#include <searpc-named-pipe-transport.h>

#if defined(_MSC_VER)
#include <windows.h>
#include <file-utils.h>
#else
#include <unistd.h>
#endif
#include <glib-object.h>
#include <cstdio>
#include <cstdlib>
#include <QLibrary>
#include <QTimer>
#include <QStringList>
#include <QString>
#include <QDebug>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>

#include "utils/utils.h"
#include "utils/process.h"
#include "seadrive-gui.h"
#include "daemon-mgr.h"
#include "settings-mgr.h"
#include "rpc/rpc-client.h"
#include "utils/utils-win.h"
#include "open-local-helper.h"
#include "i18n.h"
#if defined(Q_OS_MAC)
#include "utils/utils-mac.h"
#endif

namespace {

const int kDaemonReadyCheckIntervalMilli = 2000;
const int kMaxDaemonReadyCheck = 15;

// When the daemon process is dead, we try to restart it every 5 seconds, at
// most 10 times. The drive letter would be released by dokany driver about
// after 15 seconds after the daemon is dead.
const int kDaemonRestartInternvalMSecs = 5000;
const int kDaemonRestartMaxRetries = 10;

#if defined(Q_OS_WIN32)
const char *kSeadriveSockName = "\\\\.\\pipe\\seadrive_";
const char *kSeadriveExecutable = "seadrive.exe";
const int kDLLMissingErrorCode = -1073741515;
#else
const char *kSeadriveSockName = "seadrive.sock";
const char *kSeadriveExecutable = "seadrive";
#endif

typedef enum {
    DAEMON_INIT = 0,
    DAEMON_STARTING,
    DAEMON_CONNECTING,
    DAEMON_CONNECTED,
    DAEMON_DEAD,
    SEADRIVE_EXITING,
    MAX_STATE,
} DaemonState;

const char *DaemonStateStrs[] = {
    "init",
    "starting",
    "connecting",
    "connected",
    "dead",
    "seadrive_exiting"
};

const char *stateToStr(int state)
{
    if (state < 0 || state >= MAX_STATE) {
        return "";
    }
    return DaemonStateStrs[state];
}

} // namespace

DaemonManager::DaemonManager()
    : seadrive_daemon_(nullptr),
      searpc_pipe_client_(nullptr)
{
    current_state_ = DAEMON_INIT;
    conn_daemon_timer_ = new QTimer(this);
    connect(conn_daemon_timer_, SIGNAL(timeout()), this, SLOT(checkDaemonReady()));
    first_start_ = true;
    restart_retried_ = 0;

    connect(qApp, SIGNAL(aboutToQuit()),
            this, SLOT(seadriveExiting()));
}

DaemonManager::~DaemonManager() {
#if !defined(Q_OS_MAC)
    stopAllDaemon();
#endif
}

void DaemonManager::restartSeadriveDaemon()
{
    if (current_state_ == SEADRIVE_EXITING) {
        return;
    }

    qWarning("Trying to restart seadrive daemon");
    startSeadriveDaemon();
}

void DaemonManager::startSeadriveDaemon()
{
    if (!gui->isDevMode()) {
        shutdown_process (kSeadriveExecutable);
    }

    if (!gui->settingsManager()->getCacheDir(&current_cache_dir_))
        current_cache_dir_ = QDir(seadriveDataDir()).absolutePath();

#if defined(Q_OS_WIN32)
# if !defined(_MSC_VER)
    QLibrary dokanlib("dokan1.dll");
    if (!dokanlib.load()) {
        qWarning("dokan1.dll could not be loaded");
        gui->errorAndExit(tr("%1 failed to initialize").arg(getBrand()));
        return;
    } else {
        dokanlib.unload();
    }
#endif
    searpc_pipe_client_ = searpc_create_named_pipe_client(
        utils::win::getLocalPipeName(kSeadriveSockName).c_str());
#else
    searpc_pipe_client_ = searpc_create_named_pipe_client(
        toCStr(QDir(current_cache_dir_).filePath(kSeadriveSockName)));
#endif

    transitionState(DAEMON_STARTING);
    if (!gui->isDevMode()) {
        seadrive_daemon_ = new QProcess(this);
        connect(seadrive_daemon_, SIGNAL(started()), this, SLOT(onDaemonStarted()));
        connect(seadrive_daemon_,
                SIGNAL(finished(int, QProcess::ExitStatus)),
                this,
                SLOT(onDaemonFinished(int, QProcess::ExitStatus)));
        seadrive_daemon_->start(RESOURCE_PATH(kSeadriveExecutable), collectSeaDriveArgs());
    } else {
        qWarning() << "dev mode enabled, you are supposed to launch seadrive daemon yourself";
        transitionState(DAEMON_CONNECTING);
        conn_daemon_timer_->start(kDaemonReadyCheckIntervalMilli);
    }

}

QStringList DaemonManager::collectSeaDriveArgs()
{
    QStringList args;

    args << "-d" << current_cache_dir_;
    args << "-l" << QDir(seadriveLogDir()).absoluteFilePath("seadrive.log");
    if (I18NHelper::getInstance()->isTargetLanguage("zh_CN")) {
        args << "-L" << "zh_cn";
    } else if (I18NHelper::getInstance()->isTargetLanguage("de_de")) {
        args << "-L" << "de_de";
    } else if (I18NHelper::getInstance()->isTargetLanguage("fr_fr")) {
        args << "-L" << "fr_fr";
    }

#if defined(Q_OS_WIN32)
    QString seadrive_root = gui->seadriveRoot();
    QString sync_root_path = QDir::toNativeSeparators(seadrive_root);

    args << sync_root_path;
#endif

    auto stream = qWarning() << "starting seadrive daemon:" << kSeadriveExecutable;
    foreach (const QString& arg, args) {
        stream << arg;
    }

    return args;
}

void DaemonManager::seadriveExiting()
{
    transitionState(SEADRIVE_EXITING);
}

void DaemonManager::onDaemonStarted()
{
    qDebug("seadrive daemon is now running, checking if the service is ready");
    conn_daemon_timer_->start(kDaemonReadyCheckIntervalMilli);
    transitionState(DAEMON_CONNECTING);
    OpenLocalHelper::instance()->checkPendingOpenLocalRequest();
}

void DaemonManager::checkDaemonReady()
{
    QString str;
    static int retried = 0;
    if (searpc_named_pipe_client_connect(searpc_pipe_client_) == 0) {
        retried = 0;
        SearpcClient *rpc_client = searpc_client_with_named_pipe_transport(
            searpc_pipe_client_, "seadrive-rpcserver");
        searpc_free_client_with_pipe_transport(rpc_client);

        qDebug("seadrive daemon is ready");
        conn_daemon_timer_->stop();

        // TODO: Instead of only connecting to the rpc server, we should make a
        // real rpc call here so we can guarantee the daemon is ready to answer
        // rpc requests.
        g_usleep(1000000);

        transitionState(DAEMON_CONNECTED);

        restart_retried_ = 0;
        if (first_start_) {
            first_start_ = false;
            emit daemonStarted();
        } else {
            emit daemonRestarted();
        }
        return;
    }

    qDebug("seadrive daemon is not ready");
    if (++retried > kMaxDaemonReadyCheck) {
        qWarning("seadrive rpc is not ready after %d retry, abort", retried);
        gui->errorAndExit(tr("%1 failed to initialize").arg(getBrand()));
    }
}

void DaemonManager::stopAllDaemon()
{
    qWarning("[Daemon Mgr] stopping seadrive daemon");

    if (conn_daemon_timer_) {
        conn_daemon_timer_->stop();
        conn_daemon_timer_ = nullptr;
    }
    if (!gui->isDevMode() && seadrive_daemon_) {
#if defined(_MSC_VER)
        if (!gui->rpcClient()->exitSeadriveDaemon()) {
            qWarning("failed to exit seadrive daemon");
        }
#elif defined(Q_OS_LINUX)
        // https://forum.seafile.com/t/seadrive-transport-endpoint-is-not-connected/13240/5
        seadrive_daemon_->terminate();
#else
        seadrive_daemon_->kill();
#endif
        seadrive_daemon_->waitForFinished(1500);
        conn_daemon_timer_ = nullptr;
    }
}


void DaemonManager::onDaemonFinished(int exit_code, QProcess::ExitStatus exit_status)
{
#if defined(Q_OS_WIN32)
    if (exit_code == kDLLMissingErrorCode) {
        qWarning("seadrive exited because DLL is missing, aborting");
        conn_daemon_timer_->stop();
        gui->errorAndExit(tr("%1 failed to initialize").arg(getBrand()));
        return;
    }
#endif

    qWarning("Seadrive daemon process %s with code %d ",
             exit_status == QProcess::CrashExit ? "crashed" : "exited normally",
             exit_code);


    if (current_state_ == DAEMON_CONNECTING) {
        conn_daemon_timer_->stop();
        scheduleRestartDaemon();
    } else if (current_state_ != SEADRIVE_EXITING) {
        transitionState(DAEMON_DEAD);
        emit daemonDead();
        scheduleRestartDaemon();
    }
}

void DaemonManager::scheduleRestartDaemon()
{
    // When the daemon crashes when we first start seadrive, we should
    // not retry too many times, because during the retry nothing
    // would be shown to the user and would confuse him.
    int max_retry = 2;
    if (gui->rpcClient() && gui->rpcClient()->isConnected()) {
        max_retry = kDaemonRestartMaxRetries;
    }
    if (++restart_retried_ >= max_retry) {
        qWarning("reaching max tries of restarting seadrive daemon, aborting");
        gui->errorAndExit(tr("%1 exited unexpectedly").arg(getBrand()));
        return;
    }
    QTimer::singleShot(kDaemonRestartInternvalMSecs, this, SLOT(restartSeadriveDaemon()));
}

void DaemonManager::transitionState(int new_state)
{
    qDebug("daemon mgr: %s => %s", stateToStr(current_state_), stateToStr(new_state));
    current_state_ = new_state;
}
