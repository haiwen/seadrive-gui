#include <QTimer>

#include "kext-installer.h"
#include "osx-helperutils/osx-helperutils.h"
#include "seadrive-gui.h"

namespace
{
const int kRefreshInterval = 2 * 1000; // 2s
}

SINGLETON_IMPL(KextInstaller)

KextInstaller::KextInstaller(QObject *parent)
    : QObject(parent), install_finished_(false), kext_ready_(false)
{
    check_timer_ = new QTimer(this);
    connect(check_timer_, SIGNAL(timeout()), this, SLOT(checkKextReady()));
}

bool KextInstaller::install()
{
    if (!installHelperTool()) {
        gui->errorAndExit(
            tr("Failed to initialize: failed to install helper tool"));
        return false;
    } else if (!installKext(&install_finished_, &kext_ready_)) {
        gui->errorAndExit(
            tr("Failed to initialize: failed to install kernel driver"));
        return false;
    }

    check_timer_->start(kRefreshInterval);
    return true;
}

void KextInstaller::checkKextReady()
{
}
