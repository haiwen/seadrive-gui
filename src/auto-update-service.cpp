#include <QTimer>

#include <winsparkle.h>

#include "api/requests.h"
#include "seadrive-gui.h"
#include "utils/utils.h"

#include "auto-update-service.h"

SINGLETON_IMPL(AutoUpdateService)

namespace
{

} // namespace

AutoUpdateService::AutoUpdateService(QObject *parent) : QObject(parent)
{
}

void AutoUpdateService::start()
{
    win_sparkle_set_appcast_url("http://local.seafile.io:81/appcast.xml");
    win_sparkle_set_app_details(
        L"Seafile",
        L"Seafile Drive Client",
        QString(STRINGIZE(SEADRIVE_GUI_VERSION)).toStdWString().c_str());

    // Initialize the updater and possibly show some UI
    win_sparkle_init();
}

void AutoUpdateService::stop()
{
    win_sparkle_cleanup();
}

bool AutoUpdateService::shouldSupportAutoUpdate() const {
    // qWarning() << "shouldSupportAutoUpdate =" << (QString(getBrand()) == "SeaDrive");
    return QString(getBrand()) == "SeaDrive";
}

bool AutoUpdateService::autoUpdateEnabled() const {
    // qWarning() << "autoUpdateEnabled =" << win_sparkle_get_automatic_check_for_updates();
    return win_sparkle_get_automatic_check_for_updates();
}

void AutoUpdateService::setAutoUpdateEnabled(bool enabled) {
    // qWarning() << "setAutoUpdateEnabled:" << enabled;
    win_sparkle_set_automatic_check_for_updates(enabled ? 1 : 0);
}
