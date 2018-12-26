#include "finder-sync/finder-sync-host.h"

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
#include "utils/utils.h"
#include "utils/file-utils.h"

enum PathStatus {
    SYNC_STATUS_NONE = 0,
    SYNC_STATUS_SYNCING,
    SYNC_STATUS_ERROR,
    SYNC_STATUS_SYNCED,
    SYNC_STATUS_PARTIAL_SYNCED,
    SYNC_STATUS_CLOUD,
    SYNC_STATUS_READONLY,
    SYNC_STATUS_LOCKED,
    SYNC_STATUS_LOCKED_BY_ME,
    MAX_SYNC_STATUS,
};

namespace {
struct QtLaterDeleter {
public:
  void operator()(QObject *ptr) {
    ptr->deleteLater();
  }
};

bool parseFilePath(const QString &path,
                   QString *repo,
                   QString *path_in_repo,
                   QString *category_out)
{
    // The path of the file in relative to the mount point.
    // It is like "My Libraries/Documents"
    QString relative_path = path.mid(gui->mountDir().length() + 1);

    if (relative_path.isEmpty()) {
        return false;
    }

    if (relative_path.endsWith("/")) {
        relative_path = relative_path.left(relative_path.length() - 1);
    }

    // printf("relative_path is %s\n", toCStr(relative_path));

    if (!category_out && !relative_path.contains('/')) {
        return false;
    }

    int pos = relative_path.indexOf('/');
    QString category = relative_path.left(pos);
    if (category_out) {
        *category_out = category;
    }

    if (!relative_path.contains('/')) {
        return true;
    }

    QString remaining = relative_path.mid(pos + 1);
    // printf("category = %s, remaining = %s\n", category.toUtf8().data(), remaining.toUtf8().data());

    if (remaining.contains('/')) {
        int pos = remaining.indexOf('/');
        *repo = remaining.left(pos);
        *path_in_repo = remaining.mid(pos);
        // printf("repo = %s, path_in_repo = %s\n", repo->toUtf8().data(),
        //        path_in_repo->toUtf8().data());
    } else {
        *repo = remaining;
        *path_in_repo = "";
    }
    return true;
}

// If `category_out` is non-null, repo and path_in_repo would not be used.
bool getRepoAndRelativePath(const QString &path,
                            QString *repo,
                            QString *path_in_repo,
                            QString *category=nullptr)
{
    if (!parseFilePath(path, repo, path_in_repo, category)) {
        return false;
    }
    return !repo->isEmpty();
}

bool getCategoryFromPath(const QString& path, QString *category)
{
    QString repo;
    QString path_in_repo;
    if (!parseFilePath(path, &repo, &path_in_repo, category)) {
        return false;
    }
    return !category->isEmpty() && repo.isEmpty();
}


} // anonymous namespace

static const char *const kPathStatus[] = {
    "none", "syncing", "error", "synced", "partial_synced", "cloud", "readonly", "locked", "locked_by_me", NULL,
};

static inline PathStatus getPathStatusFromString(const QString &status) {
    for (int p = SYNC_STATUS_NONE; p < MAX_SYNC_STATUS; ++p)
        if (kPathStatus[p] == status)
            return static_cast<PathStatus>(p);
    return SYNC_STATUS_NONE;
}

// inline static bool isContainsPrefix(const QString &path,
//                                     const QString &prefix) {
//     if (prefix.size() > path.size())
//         return false;
//     if (!path.startsWith(prefix))
//         return false;
//     if (prefix.size() < path.size() && path[prefix.size()] != '/')
//         return false;
//     return true;
// }

static std::mutex update_mutex_;
// static std::vector<LocalRepo> watch_set_;
static std::vector<std::string> watch_set_;
static std::unique_ptr<GetSharedLinkRequest, QtLaterDeleter> get_shared_link_req_;
static std::unique_ptr<LockFileRequest, QtLaterDeleter> lock_file_req_;

FinderSyncHost::FinderSyncHost() : rpc_client_(new SeafileRpcClient) {
    rpc_client_->connectDaemon();
}

FinderSyncHost::~FinderSyncHost() {
    get_shared_link_req_.reset();
    lock_file_req_.reset();
}

std::string FinderSyncHost::getWatchSet()
{
    updateWatchSet(); // lock is inside

    std::unique_lock<std::mutex> lock(update_mutex_);
    QStringList repos;
    foreach (const std::string& repo, watch_set_) {
        repos << toCStr(pathJoin(gui->mountDir(), QString::fromUtf8(repo.c_str())));
    }

    auto content = repos.join("\n");
    const Account& account = gui->accountManager()->currentAccount();
    content += "\t";
    content += account.isAtLeastVersion(6, 3, 0) ? "internal-link-supported" : "internal-link-unsupported";
    return content.toUtf8().data();
}

void FinderSyncHost::updateWatchSet()
{
    std::unique_lock<std::mutex> lock(update_mutex_);

    QDir mountPoint(gui->mountDir());
    QStringList subdirs = mountPoint.entryList(
        QStringList(), QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
    watch_set_.clear();
    foreach (const QString& subdir, subdirs) {
        watch_set_.emplace_back(subdir.toUtf8().data());
    }

    lock.unlock();
}

inline QString path_concat(const QString& s1, const QString& s2)
{
    return QString("%1/%2").arg(s1).arg(s2);
}

uint32_t FinderSyncHost::getFileStatus(const QString &path)
{
    std::unique_lock<std::mutex> lock(update_mutex_);

    QString status;
    QString category;
    if (getCategoryFromPath(path, &category)) {
        if (rpc_client_->getCategorySyncStatus(category, &status) != 0) {
            return PathStatus::SYNC_STATUS_NONE;
        }
        return getPathStatusFromString(status);
    }

    QString repo;
    QString path_in_repo = "";
    if (!getRepoAndRelativePath(path, &repo, &path_in_repo, &category)) {
        return SYNC_STATUS_NONE;
    }

    if (rpc_client_->getRepoFileStatus(path_concat(category, repo), path_in_repo, &status) != 0) {
        return PathStatus::SYNC_STATUS_NONE;
    }

    // printf("path = %s, status = %s\n", toCStr(path), toCStr(status));

    return getPathStatusFromString(status);
}

void FinderSyncHost::doShareLink(const QString &path) {
    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }

    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    get_shared_link_req_.reset(new GetSharedLinkRequest(
        account, repo_id, QString("/").append(path_in_repo),
        QFileInfo(path).isFile()));

    connect(get_shared_link_req_.get(), SIGNAL(success(const QString &)), this,
            SLOT(onShareLinkGenerated(const QString &)));

    get_shared_link_req_->send();
}

void FinderSyncHost::doInternalLink(const QString &path)
{
    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }
    GetSmartLinkRequest *req = new GetSmartLinkRequest(account, repo_id, path_in_repo, QFileInfo(path).isDir());
    connect(req, SIGNAL(success(const QString&)),
            this, SLOT(onGetSmartLinkSuccess(const QString&)));
    connect(req, SIGNAL(failed(const ApiError&)),
            this, SLOT(onGetSmartLinkFailed(const ApiError&)));

    req->send();
    //SeafileLinkDialog(repo_id, account, path_in_repo, smart_link).exec();
}

void FinderSyncHost::onGetSmartLinkSuccess(const QString& smart_link)
{
    SeafileLinkDialog *dialog = new SeafileLinkDialog(smart_link);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void FinderSyncHost::onGetSmartLinkFailed(const ApiError& error)
{
    qWarning("get smart_link failed %s\n", error.toString().toUtf8().data());
}

void FinderSyncHost::doLockFile(const QString &path, bool lock)
{
    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }
    lock_file_req_.reset(new LockFileRequest(account, repo_id, path_in_repo, lock));

    connect(lock_file_req_.get(), SIGNAL(success()),
            this, SLOT(onLockFileSuccess()));
    lock_file_req_->send();
}

void FinderSyncHost::onShareLinkGenerated(const QString &link)
{
    SharedLinkDialog *dialog = new SharedLinkDialog(link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void FinderSyncHost::onLockFileSuccess()
{
    LockFileRequest* req = qobject_cast<LockFileRequest*>(sender());
    if (!req)
        return;
    rpc_client_->markFileLockState(req->repoId(), req->path(), req->lock());
}

bool FinderSyncHost::lookUpFileInformation(const QString &path,
                                           QString *ptr_repo_id,
                                           QString *ptr_path_in_repo)
{
    QString repo;
    QString category;
    if (!getRepoAndRelativePath(path, &repo, ptr_path_in_repo, &category)) {
        return false;
    }

    return rpc_client_->getRepoIdByPath(path_concat(category, repo), ptr_repo_id);
}

void FinderSyncHost::doShowFileHistory(const QString &path)
{
    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }
    QUrl url = "/repo/file_revisions/" + repo_id + "/";
    url = ::includeQueryParams(url, {{"p", path_in_repo}});
    AutoLoginService::instance()->startAutoLogin(url.toString());
}

void FinderSyncHost::doDownloadFile(const QString &path)
{
    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }
    rpc_client_->cachePath(repo_id, path_in_repo);
}
