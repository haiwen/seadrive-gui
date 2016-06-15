#include <QHash>
#include <QHostInfo>

#include "utils.h"
#include "seadrive-gui.h"
#include "api-utils.h"

namespace {

#if defined(Q_OS_WIN32)
const char *kOsName = "windows";
#elif defined(Q_OS_LINUX)
const char *kOsName = "linux";
#else
const char *kOsName = "mac";
#endif

} // namespace

QHash<QString, QString>
getSeafileLoginParams(const QString& computer_name, const QString& prefix)
{

    QHash<QString, QString> params;

    QString client_version = STRINGIZE(SEADRIVE_GUI_VERSION);
    QString computper = computer_name.isEmpty() ? QHostInfo::localHostName()
        : computer_name;

    params.insert(prefix + "platform", kOsName);
    // TODO: Find an id that doesn't change for a given computer as the device
    // id, e.g. (mac address)
    params.insert(prefix + "device_id", "4222bdbf4a676c117f445c1b20d3b60cf0b2c0ac");
    params.insert(prefix + "device_name", computer_name);
    params.insert(prefix + "client_version", client_version);
    params.insert(prefix + "platform_version", "");

    return params;
}
