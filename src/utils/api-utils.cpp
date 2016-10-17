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

    QString client_version = QString("seadrive_%1").arg(STRINGIZE(SEADRIVE_GUI_VERSION));
    QString computper = computer_name.isEmpty() ? QHostInfo::localHostName()
        : computer_name;

    params.insert(prefix + "platform", kOsName);
    params.insert(prefix + "device_id", gui->getUniqueClientId());
    params.insert(prefix + "device_name", computer_name);
    params.insert(prefix + "client_version", client_version);
    params.insert(prefix + "platform_version", "");

    return params;
}
