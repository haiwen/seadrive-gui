#include <QTimer>

#include <winsparkle.h>

#include "api/requests.h"
#include "seadrive-gui.h"
#include "utils/utils.h"

#include "auto-update-service.h"

SINGLETON_IMPL(AutoUpdateService)

namespace
{
    const char *kappcastURL = "http://localhost/appcast.xml";
    const char *kWinSparkleRegistryPath = "SOFTWARE\\Seafile\\Seafile Drive Client\\WinSparkle";
} // namespace

AutoUpdateService::AutoUpdateService(QObject *parent) : QObject(parent)
{
}

void AutoUpdateService::start()
{
    // Initialize the updater and possibly show some UI
    win_sparkle_init();
}

void AutoUpdateService::stop()
{
    win_sparkle_cleanup();
}


void AutoUpdateService::checkUpdate()
{
    win_sparkle_check_update_with_ui();
}

void AutoUpdateService::checkAndInstallUpdate() {
    win_sparkle_check_update_with_ui_and_install();
}

void AutoUpdateService::checkUpdateWithoutUI() {
    win_sparkle_check_update_without_ui();
}


void AutoUpdateService::setRequestParams() {
    win_sparkle_set_appcast_url(kappcastURL);
    win_sparkle_set_app_details(
        L"Seafile",
        L"Seafile Drive Client",
        QString(STRINGIZE(SEADRIVE_GUI_VERSION)).toStdWString().c_str());
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

uint AutoUpdateService::updateCheckInterval() const {
    return win_sparkle_get_update_check_interval();
}

void AutoUpdateService::setUpdateCheckInterval(uint interval) {
    win_sparkle_set_update_check_interval(interval);
}

// Note that @param path is relative to HKCU/HKLM root 
// and the root is not part of it. For example:
// @code
//     win_sparkle_set_registry_path("Software\\My App\\Updates");
// @endcode
void AutoUpdateService::setRegistryPath() {
    win_sparkle_set_registry_path(kWinSparkleRegistryPath);
}
