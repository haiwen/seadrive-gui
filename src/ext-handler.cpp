#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <fcntl.h>
#include <ctype.h>
#include <userenv.h>

#include <string>
#include <QMutexLocker>
#include <QList>
#include <QDir>
#include <QDebug>

#include "utils/file-utils.h"
#include "api/requests.h"
#include "ui/sharedlink-dialog.h"
#include "ui/seafilelink-dialog.h"
// #include "ui/private-share-dialog.h"
#include "rpc/rpc-client.h"
#include "api/api-error.h"
#include "seadrive-gui.h"
#include "daemon-mgr.h"
#include "account-mgr.h"
#include "settings-mgr.h"
#include "utils/utils.h"
#include "utils/utils-win.h"
#include "auto-login-service.h"
#include "ext-handler.h"

namespace {

const char *kSeafExtPipeName = "\\\\.\\pipe\\seadrive_ext_pipe_";
const int kPipeBufSize = 1024;

const quint64 kReposInfoCacheMSecs = 2000;

bool
extPipeReadN (HANDLE pipe, void *buf, size_t len)
{
    DWORD bytes_read;
    bool success = ReadFile(
        pipe,                  // handle to pipe
        buf,                   // buffer to receive data
        (DWORD)len,            // size of buffer
        &bytes_read,           // number of bytes read
        NULL);                 // not overlapped I/O

    if (!success || bytes_read != (DWORD)len) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE) {
            qDebug("[ext] connection closed by extension\n");
        } else {
            qWarning("[ext] Failed to read command from extension(), "
                     "error code %lu\n", error);
        }
        return false;
    }

    return true;
}

bool
extPipeWriteN(HANDLE pipe, void *buf, size_t len)
{
    DWORD bytes_written;
    bool success = WriteFile(
        pipe,                  // handle to pipe
        buf,                   // buffer to receive data
        (DWORD)len,            // size of buffer
        &bytes_written,        // number of bytes written
        NULL);                 // not overlapped I/O

    if (!success || bytes_written != (DWORD)len) {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE) {
            qDebug("[ext] connection closed by extension\n");
        } else {
            qWarning("[ext] Failed to read command from extension(), "
                     "error code %lu\n", error);
        }
        return false;
    }

    FlushFileBuffers(pipe);
    return true;
}

/**
 * Replace "\" with "/", and remove the trailing slash
 */
QString normalizedPath(const QString& path)
{
    QString p = QDir::fromNativeSeparators(path);
    if (p.endsWith("/")) {
        p = p.left(p.size() - 1);
    }
    return p;
}

std::string formatErrorMessage()
{
    DWORD error_code = ::GetLastError();
    if (error_code == 0) {
        return "no error";
    }
    char buf[256] = {0};
    ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL,
                    error_code,
                    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    buf,
                    sizeof(buf) - 1,
                    NULL);
    return buf;
}

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

inline QString path_concat(const QString& s1, const QString& s2)
{
    return QString("%1/%2").arg(s1).arg(s2);
}

} // namespace


SINGLETON_IMPL(SeafileExtensionHandler)

static SeafileRpcClient *rpc_client_;
static QMutex rpc_client_mutex_;

SeafileExtensionHandler::SeafileExtensionHandler()
: started_(false)
{
    listener_thread_ = new ExtConnectionListenerThread;

    connect(listener_thread_, SIGNAL(generateShareLink(const QString&, const QString&, bool, bool)),
            this, SLOT(generateShareLink(const QString&, const QString&, bool, bool)));

    connect(listener_thread_, SIGNAL(lockFile(const QString&, const QString&, bool)),
            this, SLOT(lockFile(const QString&, const QString&, bool)));

    connect(listener_thread_, SIGNAL(privateShare(const QString&, const QString&, bool)),
            this, SLOT(privateShare(const QString&, const QString&, bool)));

    connect(listener_thread_, SIGNAL(openUrlWithAutoLogin(const QUrl&)),
            this, SLOT(openUrlWithAutoLogin(const QUrl&)));

    connect(listener_thread_, SIGNAL(showLockedBy(const QString&, const QString&)),
            this, SLOT(showLockedBy(const QString&, const QString&)));

    rpc_client_ = new SeafileRpcClient();
}

void SeafileExtensionHandler::start()
{
    rpc_client_->connectDaemon();
    listener_thread_->start();
    started_ = true;

    connect(gui->daemonManager(), SIGNAL(daemonRestarted()), this, SLOT(onDaemonRestarted()));
}

void SeafileExtensionHandler::onDaemonRestarted()
{
    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_) {
        delete rpc_client_;
    }
    rpc_client_ = new SeafileRpcClient();
    rpc_client_->connectDaemon();
}

void SeafileExtensionHandler::stop()
{
    if (started_) {
        // Before seafile client exits, tell the shell to clean all the file
        // status icons
        SHChangeNotify (SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
}

void SeafileExtensionHandler::generateShareLink(const QString& repo_id,
                                                const QString& path_in_repo,
                                                bool is_file,
                                                bool internal)
{
    // qDebug("path_in_repo: %s", path_in_repo.toUtf8().data());
    const Account account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    if (internal) {
        QString path = path_in_repo;
        if (!is_file && !path.endsWith("/")) {
            path += "/";
        }
        GetSmartLinkRequest *req = new GetSmartLinkRequest(account, repo_id, path, !is_file);
        connect(req, SIGNAL(success(const QString&)),
                this, SLOT(onGetSmartLinkSuccess(const QString&)));
        connect(req, SIGNAL(failed(const ApiError&)),
                this, SLOT(onGetSmartLinkFailed(const ApiError&)));

        req->send();
    } else {
        GetSharedLinkRequest *req = new GetSharedLinkRequest(
            account, repo_id, path_in_repo);

        connect(req, SIGNAL(success(const QString&)),
                this, SLOT(onShareLinkGenerated(const QString&)));
        req->send();
    }
}

void SeafileExtensionHandler::onGetSmartLinkSuccess(const QString& smart_link)
{
    GetSmartLinkRequest *req = (GetSmartLinkRequest *)(sender());
    SeafileLinkDialog *dialog = new SeafileLinkDialog(smart_link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    req->deleteLater();
}

void SeafileExtensionHandler::onGetSmartLinkFailed(const ApiError& error)
{
    qWarning("get smart_link failed %s\n", error.toString().toUtf8().data());
}

void SeafileExtensionHandler::lockFile(const QString& repo_id,
                                       const QString& path_in_repo,
                                       bool lock)
{
    // qDebug("path_in_repo: %s", path_in_repo.toUtf8().data());
    const Account account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    LockFileRequest *req = new LockFileRequest(
        account, repo_id, path_in_repo, lock);

    connect(req, SIGNAL(success()),
            this, SLOT(onLockFileSuccess()));
    connect(req, SIGNAL(failed(const ApiError&)),
            this, SLOT(onLockFileFailed(const ApiError&)));

    req->send();
}

void SeafileExtensionHandler::privateShare(const QString& repo_id,
                                           const QString& path_in_repo,
                                           bool to_group)
{
    const Account account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        qWarning("no account found for repo %12s", repo_id.toUtf8().data());
        return;
    }

    // TODO: add back private share dialog from seafile-client

    // LocalRepo repo;
    // gui->rpcClient()->getLocalRepo(repo_id, &repo);
    // PrivateShareDialog *dialog = new PrivateShareDialog(account, repo_id, repo.name,
    //                                                     path_in_repo, to_group,
    //                                                     NULL);

    // dialog->setAttribute(Qt::WA_DeleteOnClose);
    // dialog->show();
    // dialog->raise();
    // dialog->activateWindow();
}

void SeafileExtensionHandler::openUrlWithAutoLogin(const QUrl& url)
{
    AutoLoginService::instance()->startAutoLogin(url.toString());
}

void SeafileExtensionHandler::onShareLinkGenerated(const QString& link)
{
    SharedLinkDialog *dialog = new SharedLinkDialog(link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SeafileExtensionHandler::onLockFileSuccess()
{
    LockFileRequest *req = qobject_cast<LockFileRequest *>(sender());
    // LocalRepo repo;
    // gui->rpcClient()->getLocalRepo(req->repoId(), &repo);
    // if (repo.isValid()) {
    //     gui->rpcClient()->markFileLockState(req->repoId(), req->path(), req->lock());
    //     QString path = QDir::toNativeSeparators(QDir(repo.worktree).absoluteFilePath(req->path().mid(1)));
    //     SHChangeNotify(SHCNE_ATTRIBUTES, SHCNF_PATH, path.toUtf8().data(), NULL);
    // }
}

void SeafileExtensionHandler::onLockFileFailed(const ApiError& error)
{
    LockFileRequest *req = qobject_cast<LockFileRequest *>(sender());
    QString str = req->lock() ? tr("Failed to lock file") : tr("Failed to unlock file");
    gui->warningBox(QString("%1: %2").arg(str, error.toString()));
}

void SeafileExtensionHandler::showLockedBy(const QString& repo_id, const QString& path_in_repo)
{
    // qWarning("SeafileExtensionHandler::showLockedBy is called for %s %s\n",
    //          toCStr(repo_id),
    //          toCStr(path_in_repo));
    const Account account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        return;
    }

    GetFileLockInfoRequest *req = new GetFileLockInfoRequest(
        account, repo_id, QString("/").append(path_in_repo));

    connect(req, SIGNAL(success(bool, const QString&)), this,
            SLOT(onGetFileLockInfoSuccess(bool, const QString &)));

    connect(req, SIGNAL(failed(const ApiError&)),
            this, SLOT(onGetFileLockInfoFailed(const ApiError&)));

    req->send();
}

void SeafileExtensionHandler::onGetFileLockInfoSuccess(bool found, const QString& lock_owner)
{
    // printf ("found: %s, lock_owner: %s\n", found ? "true" : "false", toCStr(lock_owner));
    GetFileLockInfoRequest *req = qobject_cast<GetFileLockInfoRequest *>(sender());
    const QString file = ::getBaseName(req->path());

    if (found) {
        gui->messageBox(tr("File \"%1\" is locked by %2").arg(file, lock_owner));
    } else {
        gui->messageBox(tr("Failed to get lock information for file \"%1\"").arg(file));
    }
    req->deleteLater();
}

void SeafileExtensionHandler::onGetFileLockInfoFailed(const ApiError& error)
{
    GetFileLockInfoRequest *req = qobject_cast<GetFileLockInfoRequest *>(sender());
    const QString file = ::getBaseName(req->path());
    gui->messageBox(tr("Failed to get lock information for file \"%1\"").arg(file));
    req->deleteLater();
}

void ExtConnectionListenerThread::run()
{
    std::string local_pipe_name = utils::win::getLocalPipeName(kSeafExtPipeName);
    qWarning("[ext listener] listening on %s", local_pipe_name.c_str());
    while (1) {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        bool connected = false;

        pipe = CreateNamedPipe(
            local_pipe_name.c_str(),  // pipe name
            PIPE_ACCESS_DUPLEX,       // read/write access
            PIPE_TYPE_MESSAGE |       // message type pipe
            PIPE_READMODE_MESSAGE |   // message-read mode
            PIPE_WAIT,                // blocking mode
            PIPE_UNLIMITED_INSTANCES, // max. instances
            kPipeBufSize,             // output buffer size
            kPipeBufSize,             // input buffer size
            0,                        // client time-out
            NULL);                    // default security attribute

        if (pipe == INVALID_HANDLE_VALUE) {
            qWarning ("Failed to create named pipe, GLE=%lu\n",
                      GetLastError());
            return;
        }

        /* listening on this pipe */
        connected = ConnectNamedPipe(pipe, NULL) ?
            true : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (!connected) {
            qWarning ("Failed on ConnectNamedPipe(), GLE=%lu\n",
                      GetLastError());
            CloseHandle(pipe);
            return;
        }

        qDebug ("[ext pipe] Accepted an extension pipe client\n");
        servePipeInNewThread(pipe);
    }
}

void ExtConnectionListenerThread::servePipeInNewThread(HANDLE pipe)
{
    ExtCommandsHandler *t = new ExtCommandsHandler(pipe);

    connect(t, SIGNAL(generateShareLink(const QString&, const QString&, bool, bool)),
            this, SIGNAL(generateShareLink(const QString&, const QString&, bool, bool)));
    connect(t, SIGNAL(lockFile(const QString&, const QString&, bool)),
            this, SIGNAL(lockFile(const QString&, const QString&, bool)));
    connect(t, SIGNAL(privateShare(const QString&, const QString&, bool)),
            this, SIGNAL(privateShare(const QString&, const QString&, bool)));
    connect(t, SIGNAL(openUrlWithAutoLogin(const QUrl&)),
            this, SIGNAL(openUrlWithAutoLogin(const QUrl&)));
    connect(t, SIGNAL(showLockedBy(const QString&, const QString&)),
            this, SIGNAL(showLockedBy(const QString&, const QString&)));
    t->start();
}

ExtCommandsHandler::ExtCommandsHandler(HANDLE pipe)
{
    pipe_ = pipe;
}

void ExtCommandsHandler::run()
{
    while (1) {
        QStringList args;
        if (!readRequest(&args)) {
            qWarning ("failed to read request from shell extension: %s",
                      formatErrorMessage().c_str());
            break;
        }

        // qWarning() << "get a new command: " << args;

        QString cmd = args.takeAt(0);
        QString resp;
        if (cmd == "list-repos") {
            resp = handleListRepos(args);
        } else if (cmd == "get-share-link") {
            handleGenShareLink(args, false);
        } else if (cmd == "get-internal-link") {
            handleGenShareLink(args, true);
        } else if (cmd == "get-file-status") {
            resp = handleGetFileStatus(args);
        } else if (cmd == "lock-file") {
            handleLockFile(args, true);
        } else if (cmd == "unlock-file") {
            handleLockFile(args, false);
        } else if (cmd == "private-share-to-group") {
            handlePrivateShare(args, true);
        } else if (cmd == "private-share-to-user") {
            handlePrivateShare(args, false);
        } else if (cmd == "show-history") {
            handleShowHistory(args);
        } else if (cmd == "show-locked-by") {
            handleShowLockedBy(args);
        } else if (cmd == "download") {
            handleDownload(args);
        } else if (cmd == "get-cached-status") {
            bool is_cached;
            handlerFileStatus(args, &is_cached);
            resp = is_cached ? "Cached" : "unCached";
            qWarning("file cached status is %s", toCStr(resp));
        } else if (cmd == "get-disk-letter") {
            resp = handlerGetDiskLetter().toLower();
        } else {
            qWarning ("[ext] unknown request command: %s", cmd.toUtf8().data());
        }

        if (!sendResponse(resp)) {
            qWarning ("failed to write response to shell extension: %s",
                      formatErrorMessage().c_str());
            break;
        }
    }

    qDebug ("An extension client is disconnected: GLE=%lu\n",
            GetLastError());
    DisconnectNamedPipe(pipe_);
    CloseHandle(pipe_);
}

bool ExtCommandsHandler::readRequest(QStringList *args)
{
    uint32_t len = 0;
    if (!extPipeReadN(pipe_, &len, sizeof(len)) || len == 0)
        return false;

    QScopedArrayPointer<char> buf(new char[len + 1]);
    buf.data()[len] = 0;
    if (!extPipeReadN(pipe_, buf.data(), len))
        return false;

    QStringList list = QString::fromUtf8(buf.data()).split('\t');
    if (list.empty()) {
        qWarning("[ext] got an empty request");
        return false;
    }
    *args = list;
    return true;
}

bool ExtCommandsHandler::sendResponse(const QString& resp)
{
    QByteArray raw_resp = resp.toUtf8();
    uint32_t len = raw_resp.length();

    if (!extPipeWriteN(pipe_, &len, sizeof(len))) {
        return false;
    }
    if (len > 0) {
        if (!extPipeWriteN(pipe_, raw_resp.data(), len)) {
            return false;
        }
    }
    return true;
}

// QList<LocalRepo> ExtCommandsHandler::listLocalRepos(quint64 ts)
// {
//     return ReposInfoCache::instance()->getReposInfo(ts);
// }

void ExtCommandsHandler::handleGenShareLink(const QStringList& args, bool internal)
{
    if (args.size() != 1) {
        return;
    }

    QString path = args[0];
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_id, &path_in_repo)) {
        return;
    }
    bool is_file = QFileInfo(path).isFile();
    emit generateShareLink(repo_id, path_in_repo, is_file, internal);

    return;
}

QString ExtCommandsHandler::handleListRepos(const QStringList& args)
{
    if (args.size() != 0) {
        qWarning("handleListRepos: args is not 0");
        return "";
    }

    const Account& account = gui->accountManager()->currentAccount();
    if (!account.isValid()) {
        qWarning("handleListRepos: account is not valid");
        return "";
    }

    QDir mount_point(gui->mountDir());
    // qWarning() << "listing directory " << gui->mountDir();
    QStringList subdirs = mount_point.entryList(
        QStringList(), QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

    QStringList fullpaths;
    QString internal_link_supported = account.isAtLeastVersion(6, 3, 0)
            ? "internal-link-supported"
            : "internal-link-unsupported";
    fullpaths << internal_link_supported;
    foreach (const QString &subdir, subdirs) {
        QStringList repos =
            QDir(pathJoin(gui->mountDir(), subdir))
                .entryList(QStringList(),
                           QDir::Dirs | QDir::NoDot | QDir::NoDotDot);
        foreach (const QString &r, repos) {
            fullpaths << pathJoin(gui->mountDir(), subdir, r);
        }
    }

    return fullpaths.join("\n");
}

QString ExtCommandsHandler::handleGetFileStatus(const QStringList& args)
{
    if (args.size() != 1) {
        return "";
    }
    QString path = args[0];

    QString status;
    QString category;
    if (getCategoryFromPath(path, &category)) {
        QMutexLocker locker(&rpc_client_mutex_);
        if (rpc_client_->getCategorySyncStatus(category, &status) != 0) {
            return "";
        }
        return status;
    }

    QString repo;
    QString path_in_repo;
    if (!getRepoAndRelativePath(path, &repo, &path_in_repo, &category)) {
        qWarning() << "failed to getRepoAndRelativePath for " << path;
        return "";
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_->getRepoFileStatus(path_concat(category, repo), path_in_repo, &status) != 0) {
        return "";
    }

    return status;
}

void ExtCommandsHandler::handleLockFile(const QStringList& args, bool lock)
{
    if (args.size() != 1) {
        return;
    }

    QString path = args[0];
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_id, &path_in_repo)) {
        return;
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_->markFileLockState(repo_id, path_in_repo, lock) == -1) {
        qWarning() << "failed to lock file " << path;
        return;
    }
    emit lockFile(repo_id, path_in_repo, lock);
}

bool ExtCommandsHandler::parseRepoFileInfo(const QString& path,
                                               QString *p_repo_id,
                                               QString *p_path_in_repo)
{
    QString category;
    QString repo;
    if (!getRepoAndRelativePath(path, &repo, p_path_in_repo, &category)) {
        qWarning() << "failed to getRepoAndRelativePath for " << path;
        return false;
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (!rpc_client_->getRepoIdByPath(path_concat(category, repo), p_repo_id)) {
        qWarning() << "failed to get the repo id for " << path;
        return false;
    }

    return true;
}

void ExtCommandsHandler::handlePrivateShare(const QStringList& args,
                                            bool to_group)
{
    if (args.size() != 1) {
        return;
    }
    QString path = normalizedPath(args[0]);
    if (!QFileInfo(path).isDir()) {
        qWarning("attempting to share %s, which is not a folder",
                 path.toUtf8().data());
        return;
    }

    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_id, &path_in_repo)) {
        return;
    }
    emit privateShare(repo_id, path_in_repo, to_group);
}

void ExtCommandsHandler::handleShowHistory(const QStringList& args)
{
    if (args.size() != 1) {
        return;
    }
    QString path = normalizedPath(args[0]);
    if (QFileInfo(path).isDir()) {
        qWarning("attempted to view history of %s, which is not a regular file",
                 path.toUtf8().data());
        return;
    }
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_id, &path_in_repo)) {
        return;
    }

    QUrl url = "/repo/file_revisions/" + repo_id + "/";
    url = ::includeQueryParams(url, {{"p", path_in_repo}});
    emit openUrlWithAutoLogin(url);
}

void ExtCommandsHandler::handleDownload(const QStringList& args)
{
    if (args.size() != 1) {
        return;
    }
    QString path = normalizedPath(args[0]);
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_id, &path_in_repo)) {
        return;
    }

    QMutexLocker locker(&rpc_client_mutex_);
    rpc_client_->cachePath(repo_id, path_in_repo);
}

void ExtCommandsHandler::handleShowLockedBy(const QStringList& args)
{
    if (args.size() != 1) {
        return;
    }
    QString path = normalizedPath(args[0]);
    if (QFileInfo(path).isDir()) {
        qDebug("attempted to view lock owner of %s, which is not a regular file",
               path.toUtf8().data());
        return;
    }

    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_id, &path_in_repo)) {
        return;
    }
    // qWarning("emitting showLockedBy\n");
    emit showLockedBy(repo_id, path_in_repo);
}


void ExtCommandsHandler::handlerFileStatus(QStringList &args, bool* is_cached) {
    if (args.size() != 1) {
        return ;
    }

    QString file_path = args.takeAt(0).replace("\\", "/");
    // TODO: delete it
    qWarning("file path is %s", toCStr(file_path));
    *is_cached = isFileCached(file_path);
}

bool ExtCommandsHandler::isFileCached(const QString &path) {
    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[ext] invalid path %s", toCStr(path));
        return false;
    }

    QMutexLocker lock(&rpc_client_mutex_);
    return rpc_client_->isFileCached(repo_id, path_in_repo);
}

bool ExtCommandsHandler::lookUpFileInformation(const QString &path,
                                               QString *ptr_repo_id,
                                               QString *ptr_path_in_repo)
{
    QString repo;
    QString category;
    if (!getRepoAndRelativePath(path, &repo, ptr_path_in_repo, &category)) {
        return false;
    }

    QMutexLocker lock(&rpc_client_mutex_);
    return rpc_client_->getRepoIdByPath(path_concat(category, repo), ptr_repo_id);
}

QString ExtCommandsHandler::handlerGetDiskLetter() {
    QString disk_letter;
    if (gui->settingsManager()->getDiskLetter(&disk_letter)) {
        return disk_letter;

    } else {
        return QString("");
    }
}
