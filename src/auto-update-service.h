#ifndef SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H
#define SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H

#include <QObject>
#include <QString>

#include "utils/singleton.h"

class QTimer;

class GetLatestVersionRequest;

// Auto update seadrive client program. Only used on windows.
class AutoUpdateService : public QObject
{
    SINGLETON_DEFINE(AutoUpdateService)
    Q_OBJECT

public:
    AutoUpdateService(QObject *parent = 0);

    bool shouldSupportAutoUpdate() const;

    bool autoUpdateEnabled() const;
    void setAutoUpdateEnabled(bool enabled);

    void start();
    void stop();
};

#endif // SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H
