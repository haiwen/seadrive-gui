#include <QTimer>

#include "account-mgr.h"
#include "seadrive-gui.h"
#include "api/requests.h"
#include "api/api-error.h"
#include "api/api-error.h"
#include "rpc/rpc-client.h"

#include "remote-wipe-service.h"

namespace {

const int kAuthPingIntervalMSecs = 1000 * 60 * 3; // 3 min
// const int kAuthPingIntervalMSecs = 1000 *  5; // 5 sec

} // namespace


SINGLETON_IMPL(RemoteWipeService)

RemoteWipeService::RemoteWipeService(QObject *parent)
    : QObject(parent),
      active_request_count_(0)
{
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, SIGNAL(timeout()), this, SLOT(sendAuthPing()));
}

RemoteWipeService::~RemoteWipeService()
{
    refresh_timer_->stop();
}

void RemoteWipeService::start()
{
    refresh_timer_->start(kAuthPingIntervalMSecs);

    sendAuthPing();
}

void RemoteWipeService::sendAuthPing()
{
    if (active_request_count_ != 0) {
        return;
    }

    qDebug("checking auth status...");

    auto accounts = gui->accountManager()->activeAccounts();
    active_request_count_ = accounts.size();

    for (int i = 0; i < accounts.size(); i++) {
        auto auth_ping_req = new AuthPingRequest(accounts.at(i));
        connect(auth_ping_req, SIGNAL(success()),
                this, SLOT(onRequestSuccess()));
        connect(auth_ping_req, SIGNAL(failed(const ApiError&)),
                this, SLOT(onRequestFailed(const ApiError&)));
        auth_ping_req->send();
    }
}

void RemoteWipeService::onRequestFailed(const ApiError& error)
{
    auto req = (AuthPingRequest *)(sender());
    req->deleteLater();

    if (req->reply()->hasRawHeader("X-Seafile-Wiped")) {
        wipeLocalFiles(req->account());
        active_request_count_--;
        return;
    }

    // for the new version of seafile server
    // we may have a 401 response whenever invalid token is used.
    // see more: https://github.com/haiwen/seahub/commit/94dcfe338a52304f5895914ac59540b6176c679e
    // but we only handle this error here to avoid complicate code since it is
    // general enough.
    if (error.type() == ApiError::HTTP_ERROR && error.httpErrorCode() == 401) {
        gui->accountManager()->removeAccount(req->account());
#ifdef Q_OS_MAC
        gui->warningBox(tr("Authorization expired, please re-login, you can find the unuploaded files at ~/Library/CloudStorage"));
#else
        gui->warningBox(tr("Authorization expired, please re-login"));
#endif
        active_request_count_--;
        return;
    }

    active_request_count_--;
}

void RemoteWipeService::onRequestSuccess()
{
    auto req = (AuthPingRequest *)(sender());
    req->deleteLater();

    active_request_count_--;
}

void RemoteWipeService::askDaemonDeleteAccount(const Account& account)
{
    if (!gui->rpcClient()->deleteAccount(account, true)) {
        qWarning() << "Failed to remove local cache of account" << account;
    }
}

void RemoteWipeService::wipeLocalFiles(const Account &account)
{
    qWarning("Got a remote wipe request, wiping local cache");
    askDaemonDeleteAccount(account);
    gui->accountManager()->clearAccountToken(account);
}
