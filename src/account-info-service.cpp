#include <QTimer>

#include "account-info-service.h"
#include "account-mgr.h"
#include "api/api-error.h"
#include "api/requests.h"
#include "seadrive-gui.h"

namespace
{
const int kRefreshInterval = 3 * 60 * 1000; // 3 min
}

SINGLETON_IMPL(AccountInfoService)

AccountInfoService::AccountInfoService(QObject* parent)
    : QObject(parent)
{
    refresh_timer_ = new QTimer(this);
    connect(refresh_timer_, SIGNAL(timeout()), this, SLOT(refresh()));
}

void AccountInfoService::start()
{
    refresh_timer_->start(kRefreshInterval);
}

void AccountInfoService::stop()
{
    refresh_timer_->stop();
}

void AccountInfoService::refresh()
{
    auto accounts = gui->accountManager()->activeAccounts();
    for (int i = 0; i < accounts.size(); i++) {
        FetchAccountInfoRequest* fetch_account_info_request = new FetchAccountInfoRequest(accounts.at(i));
        connect(fetch_account_info_request, SIGNAL(success(const AccountInfo&)), this,
                SLOT(onFetchAccountInfoSuccess(const AccountInfo&)));
        connect(fetch_account_info_request, SIGNAL(failed(const ApiError&)), this,
                SLOT(onFetchAccountInfoFailed()));
        fetch_account_info_request->send();
    }
}

void AccountInfoService::onFetchAccountInfoSuccess(const AccountInfo& info)
{
    FetchAccountInfoRequest* req = (FetchAccountInfoRequest*)(sender());
    gui->accountManager()->updateAccountInfo(req->account(), info);

    req->deleteLater();
    req = NULL;
}

void AccountInfoService::onFetchAccountInfoFailed()
{
    FetchAccountInfoRequest* req = (FetchAccountInfoRequest*)(sender());
    req->deleteLater();
    req = NULL;
}
