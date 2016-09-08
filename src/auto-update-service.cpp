#include <QTimer>

#include "api/requests.h"
#include "seadrive-gui.h"
#include "utils/utils.h"

#include "auto-update-service.h"

SINGLETON_IMPL(AutoUpdateService)

namespace {

// First check happens at 5 minutes after program startup.
// const int kFirstVersionCheckDelayMSec = 300 * 1000;

const int kFirstVersionCheckDelayMSec = 5 * 1000;

// Check the latest version every hour.
const int kCheckLatestVersionIntervalMSec = 3600 * 1000;

}  // namespace

AutoUpdateService::AutoUpdateService(QObject *parent) : QObject(parent)
{
    req_ = nullptr;

    check_timer_ = new QTimer(this);
    connect(check_timer_, SIGNAL(timeout()),
            this, SLOT(checkLatestVersion()));
}

void AutoUpdateService::start()
{
    QTimer::singleShot(kFirstVersionCheckDelayMSec,
                       this, SLOT(checkLatestVersion()));
    check_timer_->start(kCheckLatestVersionIntervalMSec);
}

void AutoUpdateService::checkLatestVersion()
{
    if (req_) {
        return;
    }

    req_ = new GetLatestVersionRequest(gui->getUniqueClientId());
    connect(req_, SIGNAL(success(const QString&)),
            this, SLOT(onGetLatestVersionSuccess(const QString&)));
    req_->send();
}

void AutoUpdateService::onGetLatestVersionSuccess(const QString &version)
{
    printf ("latest version is %s\n", version.toUtf8().data());
    req_->deleteLater();
    req_ = nullptr;
}
