#ifndef SEADRIVE_GUI_SERVER_CONNECTIVITY_SERVICE_H
#define SEADRIVE_GUI_SERVER_CONNECTIVITY_SERVICE_H

#include <QObject>
#include <QUrl>
#include <QHash>

#include "utils/singleton.h"

class PingServerRequest;
class ApiError;

/**
 * Tracks when each server was last successfully contacted, so that
 * network errors reported by the daemon can be treated as resolved once
 * the server is reachable again.
 *
 * The last-success time is updated passively by successful API requests
 * (see SeafileApiClient), and actively by checkServer(), which the tray
 * icon calls while a network error is being displayed.
 */
class ServerConnectivityService : public QObject
{
    Q_OBJECT
    SINGLETON_DEFINE(ServerConnectivityService)
public:
    // Seconds since epoch of the last successful contact with the
    // server, or 0 if it has not been contacted successfully yet.
    qint64 lastSuccessTime(const QString& host) const;

    // Ping the server unless a check is already in flight or one was
    // attempted recently.
    void checkServer(const QUrl& url);

public slots:
    void updateOnSuccessfulRequest(const QUrl& url);

private slots:
    void onPingServerSuccess();
    void onPingServerFailed();

private:
    Q_DISABLE_COPY(ServerConnectivityService)
    ServerConnectivityService(QObject *parent=0);

    QHash<QString, qint64> last_success_;
    QHash<QString, qint64> last_attempt_;
    QHash<QString, PingServerRequest *> requests_;
};

#endif // SEADRIVE_GUI_SERVER_CONNECTIVITY_SERVICE_H
