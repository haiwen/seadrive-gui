#include <QEventLoop>
#include <QTimer>

#include "kext-installer.h"
#include "osx-helperutils/osx-helperutils.h"
#include "seadrive-gui.h"

namespace
{
const int kCheckInterval = 2 * 1000; // 2s
const int kMaxRetries = 5; // 5 * 2s = 10s
}

SINGLETON_IMPL(KextInstaller)

KextInstaller::KextInstaller(QObject *parent)
    : QObject(parent), install_finished_(false), kext_ready_(false), retried_(0)
{
    check_timer_ = new QTimer(this);
    connect(check_timer_, SIGNAL(timeout()), this, SLOT(checkKextReady()));
}

bool KextInstaller::install()
{
    qWarning("start to install the helper tool");
    if (!installHelperTool()) {
        qWarning("failed to install helper tool");
        return false;
    }

    // qWarning("start to install/load the kernel driver");
    // if (!installKext(&install_finished_, &kext_ready_)) {
    //     qWarning("failed to install the kernel driverkext");
    //     return false;
    // }

    check_timer_->start(kCheckInterval);

    // See
    // https://doc.qt.io/archives/qq/qq27-responsive-guis.html#waitinginalocaleventloop
    QEventLoop q;
    connect(this, SIGNAL(checkDone()), &q, SLOT(quit()));
    q.exec();

    printf("after processEvents\n");
    return kext_ready_;
}

void KextInstaller::checkKextReady()
{
    printf("checkKextReady: install_finished_ = %s\n",
           install_finished_ ? "true" : "false");
    if (!install_finished_) {
        retried_ += 1;
        if (retried_ > kMaxRetries) {
            install_finished_ = true;
            kext_ready_ = false;
            check_timer_->stop();
            emit checkDone();
            return;
        }
        return;
    } else {
        check_timer_->stop();
        emit checkDone();
        return;
    }
}
