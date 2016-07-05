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
    SYNC_STATUS_IGNORED,
    SYNC_STATUS_SYNCED,
    SYNC_STATUS_READONLY,
    SYNC_STATUS_PAUSED,
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
} // anonymous namespace

static const char *const kPathStatus[] = {
    "none", "syncing", "error", "ignored", "synced", "readonly", "paused", "locked", "locked_by_me", NULL,
};

static inline PathStatus getPathStatusFromString(const QString &status) {
    for (int p = SYNC_STATUS_NONE; p < MAX_SYNC_STATUS; ++p)
        if (kPathStatus[p] == status)
            return static_cast<PathStatus>(p);
    return SYNC_STATUS_NONE;
}

inline static bool isContainsPrefix(const QString &path,
                                    const QString &prefix) {
    if (prefix.size() > path.size())
        return false;
    if (!path.startsWith(prefix))
        return false;
    if (prefix.size() < path.size() && path[prefix.size()] != '/')
        return false;
    return true;
}

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

uint32_t FinderSyncHost::getFileStatus(const char *repo_id, const char *path) {
    std::unique_lock<std::mutex> lock(update_mutex_);

    QString repo = QString::fromUtf8(repo_id, 36);
    QString path_in_repo = path;
    QString status;
    bool isDirectory = path_in_repo.endsWith('/');
    if (isDirectory)
        path_in_repo.resize(path_in_repo.size() - 1);
    if (rpc_client_->getRepoFileStatus(
            repo,
            path_in_repo,
            isDirectory, &status) != 0) {
        return PathStatus::SYNC_STATUS_NONE;
    }

    return getPathStatusFromString(status);
}

void FinderSyncHost::doShareLink(const QString &path) {
    QString repo_id;
    Account account;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &account, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
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
    QString repo_id;
    Account account;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &account, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }
    SeafileLinkDialog(repo_id, account, path_in_repo).exec();
}

void FinderSyncHost::doLockFile(const QString &path, bool lock)
{
    QString repo_id;
    Account account;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &account, &path_in_repo)) {
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

bool FinderSyncHost::lookUpFileInformation(const QString &path, QString *repo_id, Account *account, QString *path_in_repo)
{
    // TODO: Implement it for seadrive
    return false;

    // QString worktree;
    // // work in a mutex
    // {
    //     std::unique_lock<std::mutex> watch_set_lock(update_mutex_);
    //     for (const LocalRepo &repo : watch_set_)
    //         if (isContainsPrefix(path, repo.worktree)) {
    //             *repo_id = repo.id;
    //             worktree = repo.worktree;
    //             break;
    //         }
    // }
    // if (worktree.isEmpty() || repo_id->isEmpty())
    //     return false;

    // *path_in_repo = QDir(worktree).relativeFilePath(path);
    // if (path.endsWith("/"))
    //     *path_in_repo += "/";

    // // we have a empty path_in_repo representing the root of the directory,
    // // and we are okay!
    // if (path_in_repo->startsWith("."))
    //     return false;

    // *account = gui->accountManager()->getAccountByRepo(*repo_id);
    // if (!account->isValid())
    //     return false;

    // return true;
}

void FinderSyncHost::doShowFileHistory(const QString &path)
{
    QString repo_id;
    Account account;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &account, &path_in_repo)) {
        qWarning("[FinderSync] invalid path %s", path.toUtf8().data());
        return;
    }
    QUrl url = "/repo/file_revisions/" + repo_id + "/";
    url = ::includeQueryParams(url, {{"p", path_in_repo}});
    AutoLoginService::instance()->startAutoLogin(url.toString());
}
