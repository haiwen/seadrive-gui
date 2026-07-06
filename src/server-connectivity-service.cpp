#include <QDateTime>

#include "server-connectivity-service.h"
#include "api/api-error.h"
#include "api/requests.h"

namespace {

const int kMinCheckIntervalMSecs = 10 * 1000;

} // namespace

SINGLETON_IMPL(ServerConnectivityService)

ServerConnectivityService::ServerConnectivityService(QObject *parent)
    : QObject(parent)
{
}

qint64 ServerConnectivityService::lastSuccessTime(const QString& host) const
{
    return last_success_.value(host, 0);
}

void ServerConnectivityService::checkServer(const QUrl& url)
{
    const QString host = url.host();
    if (host.isEmpty() || requests_.contains(host)) {
        return;
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - last_attempt_.value(host, 0) < kMinCheckIntervalMSecs) {
        return;
    }
    last_attempt_[host] = now;

    PingServerRequest *req = new PingServerRequest(url);
    connect(req, SIGNAL(success()),
            this, SLOT(onPingServerSuccess()));
    connect(req, SIGNAL(failed(const ApiError&)),
            this, SLOT(onPingServerFailed()));
    req->send();
    requests_[host] = req;
}

void ServerConnectivityService::updateOnSuccessfulRequest(const QUrl& url)
{
    if (url.host().isEmpty()) {
        return;
    }
    last_success_[url.host()] = QDateTime::currentMSecsSinceEpoch() / 1000;
}

void ServerConnectivityService::onPingServerSuccess()
{
    PingServerRequest *req = (PingServerRequest *)sender();
    last_success_[req->url().host()] = QDateTime::currentMSecsSinceEpoch() / 1000;
    requests_.remove(req->url().host());
    req->deleteLater();
}

void ServerConnectivityService::onPingServerFailed()
{
    PingServerRequest *req = (PingServerRequest *)sender();
    requests_.remove(req->url().host());
    req->deleteLater();
}
