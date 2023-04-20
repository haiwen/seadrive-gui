#include <vector>
#include <mutex>
#include <memory>

#include <QDir>
#include <QFileInfo>

#include "account.h"
#include "account-mgr.h"
#include "auto-login-service.h"
#include "settings-mgr.h"
#include "seadrive-gui.h"
#include "rpc/rpc-client.h"
#include "api/requests.h"
#include "ui/sharedlink-dialog.h"
#include "ui/seafilelink-dialog.h"
#include "ui/uploadlink-dialog.h"
#include "utils/utils.h"
#include "utils/file-utils.h"

#include "sync-command.h"

namespace {
struct QtLaterDeleter {
public:
  void operator()(QObject *ptr) {
    ptr->deleteLater();
  }
};
} // anonymous namespace

static std::unique_ptr<GetSharedLinkRequest, QtLaterDeleter> get_shared_link_req_;
static std::unique_ptr<GetUploadLinkRequest, QtLaterDeleter> get_upload_link_req_;
static std::unique_ptr<GetSmartLinkRequest, QtLaterDeleter> get_smart_link_req_;

SyncCommand::SyncCommand() {
}

SyncCommand::~SyncCommand() {
    get_shared_link_req_.reset();
    get_upload_link_req_.reset();
    get_smart_link_req_.reset();
}

void SyncCommand::doShareLink(const Account &account, const QString &repo_id, const QString &path) {
    QString encoded_path = path.toUtf8().toPercentEncoding();
    get_shared_link_req_.reset(new GetSharedLinkRequest(
        account, repo_id, QString("/").append(encoded_path)));

    connect(get_shared_link_req_.get(), SIGNAL(success(const QString &)), this,
            SLOT(onShareLinkGenerated(const QString &)));
    connect(get_shared_link_req_.get(), SIGNAL(failed(const ApiError &)), this,
            SLOT(onShareLinkGeneratedFailed(const ApiError &)));

    get_shared_link_req_->send();
}

void SyncCommand::onShareLinkGenerated(const QString &link)
{
    GetSharedLinkRequest *req = qobject_cast<GetSharedLinkRequest *>(sender());
    const Account account = req->getAccount();
    const QString repo_id = req->getRepoId();
    const QString repo_path = req->getRepoPath();

    SharedLinkDialog *dialog = new SharedLinkDialog(link, account, repo_id, repo_path, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SyncCommand::onShareLinkGeneratedFailed(const ApiError& error)
{
    int http_error_code = error.httpErrorCode();
    if (http_error_code == 403) {
        gui->warningBox(tr("No permissions to create a shared link"));
    } else {
        gui->warningBox(tr("failed to get share link %1").arg(error.toString()));
    }
}

void SyncCommand::doInternalLink(const Account &account, const QString &repo_id, const QString &path, bool is_dir)
{
    get_smart_link_req_.reset(new GetSmartLinkRequest(
        account, repo_id, QString("/").append(path), is_dir));
    connect(get_smart_link_req_.get(), SIGNAL(success(const QString&)),
            this, SLOT(onGetSmartLinkSuccess(const QString&)));
    connect(get_smart_link_req_.get(), SIGNAL(failed(const ApiError&)),
            this, SLOT(onGetSmartLinkFailed(const ApiError&)));

    get_smart_link_req_->send();
}

void SyncCommand::onGetSmartLinkSuccess(const QString& smart_link)
{
    SeafileLinkDialog *dialog = new SeafileLinkDialog(smart_link);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SyncCommand::onGetSmartLinkFailed(const ApiError& error)
{
    int http_error_code =  error.httpErrorCode();
    if (http_error_code == 403) {
        gui->warningBox(tr("No permissions to create a shared link"));
    } else {
        gui->warningBox(tr("failed get internal link %1").arg(error.toString()));
    }
    qWarning("get smart_link failed %s\n", error.toString().toUtf8().data());
}

void SyncCommand::doShowFileHistory(const Account &account, const QString &repo_id, const QString &path)
{
    QUrl url = "/repo/file_revisions/" + repo_id + "/";
    url = ::includeQueryParams(url, {{"p", path}});
    AutoLoginService::instance()->startAutoLogin(account, url.toString());
}

void SyncCommand::doGetUploadLink(const Account &account, const QString &repo_id, const QString &path)
{
    get_upload_link_req_.reset(new GetUploadLinkRequest(
            account, repo_id, QString("/").append(path)));

    connect(get_upload_link_req_.get(), SIGNAL(success(const QString&)), this,
            SLOT(onGetUploadLinkSuccess(const QString &)));
    connect(get_upload_link_req_.get(), SIGNAL(failed(const ApiError&)), this,
            SLOT(onGetUploadLinkFailed(const ApiError&)));

    get_upload_link_req_->send();
}

void SyncCommand::onGetUploadLinkSuccess(const QString& upload_link)
{
    UploadLinkDialog *dialog = new UploadLinkDialog(upload_link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SyncCommand::onGetUploadLinkFailed(const ApiError& error)
{
    const QString file = ::getBaseName(get_upload_link_req_->path());
    gui->messageBox(tr("Failed to get upload link for file \"%1\"").arg(file));
}
