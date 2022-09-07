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
      auth_ping_req_(nullptr),
      in_refresh_(false),
      wipe_in_progress_(false)
{
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, SIGNAL(timeout()), this, SLOT(sendAuthPing()));
}

RemoteWipeService::~RemoteWipeService()
{
    refresh_timer_->stop();
    if (auth_ping_req_) {
        auth_ping_req_->deleteLater();
    }
}

void RemoteWipeService::start()
{
    refresh_timer_->start(kAuthPingIntervalMSecs);

    connect(gui->accountManager(), SIGNAL(accountsChanged()),
            this, SLOT(onAccountChanged()));
    sendAuthPing();
}

void RemoteWipeService::onAccountChanged()
{
    sendAuthPing(true);
}

void RemoteWipeService::sendAuthPing()
{
    if (in_refresh_) {
        return;
    }

    qDebug("checking auth status...");

    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    in_refresh_ = true;

    if (auth_ping_req_) {
        auth_ping_req_->deleteLater();
    }
    auth_ping_req_ = new AuthPingRequest(account);

    connect(auth_ping_req_, SIGNAL(success()),
            this, SLOT(onRequestSuccess()));
    connect(auth_ping_req_, SIGNAL(failed(const ApiError&)),
            this, SLOT(onRequestFailed(const ApiError&)));

    auth_ping_req_->send();
}

void RemoteWipeService::onRequestFailed(const ApiError& error)
{
    in_refresh_ = false;

    if (auth_ping_req_->reply()->hasRawHeader("X-Seafile-Wiped")) {
        qWarning ("current device is marked to be remote wiped\n");
        if (!wipe_in_progress_) {
            wipe_in_progress_ = true;
            // Wipe the local cache and ask the user to relogin.
            wipeLocalFiles();
        }
        return;
    }

    // for the new version of seafile server
    // we may have a 401 response whenever invalid token is used.
    // see more: https://github.com/haiwen/seahub/commit/94dcfe338a52304f5895914ac59540b6176c679e
    // but we only handle this error here to avoid complicate code since it is
    // general enough.
    if (error.type() == ApiError::HTTP_ERROR && error.httpErrorCode() == 401) {
        askDaemonDeleteAccount();
        gui->warningBox(tr("Authorization expired, please re-login"));
        gui->accountManager()->invalidateCurrentLogin();
        return;
    }
}

void RemoteWipeService::onRequestSuccess()
{
    in_refresh_ = false;
}

void RemoteWipeService::sendAuthPing(bool force)
{
    if (force) {
        in_refresh_ = false;
    }

    sendAuthPing();
}

void RemoteWipeService::askDaemonDeleteAccount()
{
    const Account& account = gui->accountManager()->currentAccount();
    if (!gui->rpcClient()->deleteAccount(account)) {
        qWarning() << "Failed to remove local cache of account" << account;
    }
}

void RemoteWipeService::wipeLocalFiles()
{
    qWarning("Got a remote wipe request, wiping local cache");
    askDaemonDeleteAccount();
    gui->accountManager()->clearAccountToken(gui->accountManager()->currentAccount());
    wipe_in_progress_ = false;
}
