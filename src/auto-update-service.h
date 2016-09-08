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

    void start();

private slots:
    void checkLatestVersion();
    void onGetLatestVersionSuccess(const QString &version);

private:
    QTimer *check_timer_;

    GetLatestVersionRequest *req_;
};

#endif // SEADRIVE_GUI_AUTO_UPDATE_SERVICE_H
