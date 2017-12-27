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

bool getRepoAndRelativePath(const QString &path,
                            QString *repo,
                            QString *path_in_repo)
{
    // The path of the file in relative to the mount point.
    QString relative_path = path.mid(gui->mountDir().length() + 1);

    if (relative_path.isEmpty()) {
        return false;
    }

    if (relative_path.endsWith("/")) {
        relative_path = relative_path.left(relative_path.length() - 1);
    }

    // printf("relative_path is %s\n", toCStr(relative_path));

    if (relative_path.contains('/')) {
        int pos = relative_path.indexOf('/');
        *repo = relative_path.left(pos);
        *path_in_repo = relative_path.mid(pos);
        // printf("repo = %s, path_in_repo = %s\n", repo.toUtf8().data(),
        // path_in_repo.toUtf8().data());
    } else {
        *repo = relative_path;
        *path_in_repo = "";
    }
    return true;
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

    return repos.join("\n").toUtf8().data();
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

uint32_t FinderSyncHost::getFileStatus(const QString &path)
{
    std::unique_lock<std::mutex> lock(update_mutex_);

    QString repo;
    QString path_in_repo = "";
    if (!getRepoAndRelativePath(path, &repo, &path_in_repo)) {
        return SYNC_STATUS_NONE;
    }

    QString status;
    if (rpc_client_->getRepoFileStatus(repo, path_in_repo, &status) != 0) {
        return PathStatus::SYNC_STATUS_NONE;
    }

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
    SeafileLinkDialog *dialog = new SeafileLinkDialog(repo_id, account, path_in_repo);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
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
    if (!getRepoAndRelativePath(path, &repo, ptr_path_in_repo)) {
        return false;
    }

    return rpc_client_->getRepoIdByPath(repo, ptr_repo_id);
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
