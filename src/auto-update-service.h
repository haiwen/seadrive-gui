#ifndef SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H
#define SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H

#if defined(HAVE_SPARKLE_SUPPORT)
#include <QObject>
#include <QString>

#include "utils/singleton.h"

class AutoUpdateAdapter;

// Auto update seafile client program. Only used on windows/mac.
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

    void checkUpdate();

private:
    void enableUpdateByDefault();
    QString getAppcastURI();
    AutoUpdateAdapter *adapter_;
};

#endif // HAVE_SPARKLE_SUPPORT
#endif // SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H

