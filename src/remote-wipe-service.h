#ifndef SEADRIVE_GUI_REMOTE_WIPE_SERVICE_H_
#define SEADRIVE_GUI_REMOTE_WIPE_SERVICE_H_

#include <QObject>

#include "utils/singleton.h"
#include "api/requests.h"

class QTimer;

class ApiError;
class AuthPingRequest;

class RemoteWipeService: public QObject
{
    SINGLETON_DEFINE(RemoteWipeService)
    Q_OBJECT
public:
    ~RemoteWipeService();

    void start();
    void sendAuthPing(bool force);

public slots:
    void sendAuthPing();

signals:
    void notificationsChanged();

private slots:
    void onRequestSuccess();
    void onRequestFailed(const ApiError& error);
    void onAccountChanged();

private:
    RemoteWipeService(QObject *parent=0);
    void wipeLocalFiles();
    void askDaemonDeleteAccount(const Account& account);

    QTimer *refresh_timer_;
    AuthPingRequest *auth_ping_req_;
    bool in_refresh_;

    bool wipe_in_progress_;
};

#endif // SEADRIVE_GUI_REMOTE_WIPE_SERVICE_H_
