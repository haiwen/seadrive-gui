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
#include "ui/uploadlink-dialog.h"
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
#include "thumbnail-service.h"

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
                   Account *account,
                   QString *repo,
                   QString *path_in_repo,
                   QString *category_out)
{
    // The path of the file in relative to the mount point.
    // It is like "My Libraries/Documents"
    QString relative_path;

    auto accounts = gui->accountManager()->activeAccounts();
    for (auto a : accounts) {
        auto root = QDir::cleanPath(a.syncRoot) + "/";
        if (path.startsWith(root)) {
            relative_path = path.mid(root.length());
            *account = a;
            break;
        }
    }

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

    connect(listener_thread_, SIGNAL(generateShareLink(const Account&, const QString&, const QString&, bool, bool)),
            this, SLOT(generateShareLink(const Account&, const QString&, const QString&, bool, bool)));

    connect(listener_thread_, SIGNAL(lockFile(const Account&, const QString&, const QString&, bool)),
            this, SLOT(lockFile(const Account&, const QString&, const QString&, bool)));

    connect(listener_thread_, SIGNAL(privateShare(const Account&, const QString&, const QString&, bool)),
            this, SLOT(privateShare(const Account&, const QString&, const QString&, bool)));

    connect(listener_thread_, SIGNAL(openUrlWithAutoLogin(const Account&, const QUrl&)),
            this, SLOT(openUrlWithAutoLogin(const Account&, const QUrl&)));

    connect(listener_thread_, SIGNAL(showLockedBy(const Account&, const QString&, const QString&)),
            this, SLOT(showLockedBy(const Account&, const QString&, const QString&)));

    connect(listener_thread_, &ExtConnectionListenerThread::getUploadLink,
            this, &SeafileExtensionHandler::getUploadLink);

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

void SeafileExtensionHandler::getUploadLink(const Account& account, const QString& repo_id, const QString& path_in_repo)
{
    GetUploadLinkRequest *req = new GetUploadLinkRequest(
            account, repo_id, "/" + path_in_repo);
    connect(req, &GetUploadLinkRequest::success,
            this,&SeafileExtensionHandler::onGetUploadLinkSuccess);
    connect(req, &GetUploadLinkRequest::failed,
            this,&SeafileExtensionHandler::onGetUploadLinkFailed);
    req->send();
}

void SeafileExtensionHandler::onGetUploadLinkSuccess(const QString& upload_link)
{
    UploadLinkDialog *dialog = new UploadLinkDialog(upload_link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SeafileExtensionHandler::onGetUploadLinkFailed(const ApiError& error)
{
    GetUploadLinkRequest *req = qobject_cast<GetUploadLinkRequest *>(sender());
    const QString file = ::getBaseName(req->path());
    gui->messageBox(tr("Failed to get upload link information for file \"%1\"").arg(file));
    req->deleteLater();
}

void SeafileExtensionHandler::generateShareLink(const Account& account,
                                                const QString& repo_id,
                                                const QString& path_in_repo,
                                                bool is_file,
                                                bool internal)
{
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
        QString encoded_path_in_repo = path_in_repo.toUtf8().toPercentEncoding();
        GetSharedLinkRequest *req = new GetSharedLinkRequest(
            account, repo_id, encoded_path_in_repo);

        connect(req, SIGNAL(success(const QString&)),
                this, SLOT(onShareLinkGenerated(const QString&)));

        connect(req, SIGNAL(failed(const ApiError&)),
                this, SLOT(onShareLinkGeneratedFailed(const ApiError&)));
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

    int http_error_code =  error.httpErrorCode();
    if (http_error_code == 403) {
        gui->warningBox(tr("No permissions to create a shared link"));
    } else {
        gui->warningBox(tr("failed get internal link %1").arg(error.toString()));
    }
}

void SeafileExtensionHandler::onShareLinkGenerated(const QString& link)
{
    GetSharedLinkRequest *req = qobject_cast<GetSharedLinkRequest *>(sender());
    const Account account = req->getAccount();
    const QString repo_id = req->getRepoId();
    const QString repo_path = req->getRepoPath();

    SharedLinkDialog *dialog = new SharedLinkDialog(link,
                                                    account,
                                                    repo_id,
                                                    repo_path,
                                                    NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SeafileExtensionHandler::onShareLinkGeneratedFailed(const ApiError& error) {
    int http_error_code = error.httpErrorCode();
    if (http_error_code == 403) {
        gui->warningBox(tr("No permissions to create a shared link"));
    } else {
        gui->messageBox(tr("Failed to get share link %1\n").arg(error.toString()));
    }
}

void SeafileExtensionHandler::lockFile(const Account& account,
                                       const QString& repo_id,
                                       const QString& path_in_repo,
                                       bool lock)
{
    LockFileRequest *req = new LockFileRequest(
        account, repo_id, path_in_repo, lock);

    connect(req, SIGNAL(success()),
            this, SLOT(onLockFileSuccess()));
    connect(req, SIGNAL(failed(const ApiError&)),
            this, SLOT(onLockFileFailed(const ApiError&)));

    req->send();
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

void SeafileExtensionHandler::privateShare(const Account& account,
                                           const QString& repo_id,
                                           const QString& path_in_repo,
                                           bool to_group)
{
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

void SeafileExtensionHandler::openUrlWithAutoLogin(const Account& account,
                                                   const QUrl& url)
{
    AutoLoginService::instance()->startAutoLogin(account, url.toString());
}

void SeafileExtensionHandler::showLockedBy(const Account& account, const QString& repo_id, const QString& path_in_repo)
{
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

    connect(t, SIGNAL(generateShareLink(const Account&, const QString&, const QString&, bool, bool)),
            this, SIGNAL(generateShareLink(const Account&, const QString&, const QString&, bool, bool)));
    connect(t, SIGNAL(lockFile(const Account&, const QString&, const QString&, bool)),
            this, SIGNAL(lockFile(const Account&, const QString&, const QString&, bool)));
    connect(t, SIGNAL(privateShare(const Account&, const QString&, const QString&, bool)),
            this, SIGNAL(privateShare(const Account&, const QString&, const QString&, bool)));
    connect(t, SIGNAL(openUrlWithAutoLogin(const Account&, const QUrl&)),
            this, SIGNAL(openUrlWithAutoLogin(const Account&, const QUrl&)));
    connect(t, SIGNAL(showLockedBy(const Account&, const QString&, const QString&)),
            this, SIGNAL(showLockedBy(const Account&, const QString&, const QString&)));
    connect(t, &ExtCommandsHandler::getUploadLink,
            this, &ExtConnectionListenerThread::getUploadLink);
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

        qWarning() << "get a new command: " << args;

        QString cmd = args.takeAt(0);
        QString resp;
        if (cmd == "list-repos") {
            resp = handleListRepos(args);
        } else if (cmd == "get-share-link") {
            handleGenShareLink(args, false);
        } else if (cmd == "get-internal-link") {
            handleGenShareLink(args, true);
        } else if (cmd == "get-file-status") {
            resp = handleGetFileLockStatus(args);
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
        } else if (cmd == "get-upload-link") {
            handleGetUploadLink(args);
        } else if (cmd == "download") {
            handleDownload(args);
        } else if (cmd == "get-cached-status") {
            bool is_cached;
            handlerFileStatus(args, &is_cached);
            resp = is_cached ? "cached" : "uncached";
            // TODO: delete it
            qWarning("file cached status is %s", toCStr(resp));
        } else if (cmd == "get-thumbnail-from-server") {
        // TODO: get seafile server from server;
            qWarning ("[ext] begin to get thumbnail");
            resp = handlerGetThumbnailFromServer(args);
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

    Account account;
    QString path = args[0];
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return;
    }
    bool is_file = QFileInfo(path).isFile();
    emit generateShareLink(account, repo_id, path_in_repo, is_file, internal);

    return;
}

QString ExtCommandsHandler::handleListRepos(const QStringList& args)
{
    if (args.size() != 0) {
        qWarning("handleListRepos: args is not 0");
        return "";
    }

    QStringList fullpaths;
    fullpaths << "internal-link-supported";

    auto accounts = gui->accountManager()->activeAccounts();
    for (auto account : accounts) {
        fullpaths << account.syncRoot;

        auto subdirs = QDir(account.syncRoot).entryList(
            QStringList(), QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

        for (auto subdir : subdirs) {
            auto repos = QDir(pathJoin(account.syncRoot, subdir)).entryList(
                QStringList(), QDir::Dirs | QDir::NoDot | QDir::NoDotDot);

            for (auto repo : repos) {
                fullpaths << pathJoin(account.syncRoot, subdir, repo);
            }
        }
    }

    return fullpaths.join("\n");
}

void ExtCommandsHandler::handleGetUploadLink(const QStringList& args)
{
    if (args.size() != 1) {
        return;
    }
    QString path = normalizedPath(args[0]);
    Account account;
    QString repo_id, path_in_repo;

    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return;
    }

    emit getUploadLink(account, repo_id, path_in_repo);
}

QString ExtCommandsHandler::handleGetFileLockStatus(const QStringList& args)
{
    if (args.size() != 1) {
        return "";
    }
    QString path = args[0];

    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return"";
    }


    QMutexLocker locker(&rpc_client_mutex_);
    int lock_status;
    if (!rpc_client_->getRepoFileLockStatus(repo_id, path_in_repo, &lock_status)) {
        qWarning() << "failed to file lock status" << path;
        return"";
    }

    QString status = "none";
    switch (lock_status) {
    case NONE:
        status = "none";
        break;
    case LOCKED_BY_OTHER:
        status = "locked";
        break;
    case LOCKED_BY_OWNER:
        status = "locked_by_me";
        break;
    case LOCKED_AUTO:
        status = "locked_auto";
        break;
    default:
        qWarning() << "unknown locked status";
    }

    return status;
}

void ExtCommandsHandler::handleLockFile(const QStringList& args, bool lock)
{
    if (args.size() != 1) {
        return;
    }

    QString path = args[0];
    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return;
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_->markFileLockState(repo_id, path_in_repo, lock) == -1) {
        qWarning() << "failed to lock file " << path;
        return;
    }
    emit lockFile(account, repo_id, path_in_repo, lock);
}

bool ExtCommandsHandler::parseRepoFileInfo(const QString& path,
                                               Account *p_account,
                                               QString *p_repo_id,
                                               QString *p_path_in_repo)
{
    QString repo;
    QString category;
    if (!parseFilePath(path, p_account, &repo, p_path_in_repo, &category)) {
        qWarning() << "failed to parse file path:" << path;
        return false;
    }
    if (repo.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (!rpc_client_->getRepoIdByPath(p_account->serverUrl.url(),
                                      p_account->username,
                                      path_concat(category, repo),
                                      p_repo_id)) {
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

    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return;
    }
    emit privateShare(account, repo_id, path_in_repo, to_group);
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
    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return;
    }

    QUrl url = "/repo/file_revisions/" + repo_id + "/";
    url = ::includeQueryParams(url, {{"p", path_in_repo}});
    emit openUrlWithAutoLogin(account, url);
}

void ExtCommandsHandler::handleDownload(const QStringList& args)
{
    if (args.size() != 1) {
        return;
    }
    QString path = normalizedPath(args[0]);
    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
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

    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return;
    }
    // qWarning("emitting showLockedBy\n");
    emit showLockedBy(account, repo_id, path_in_repo);
}


void ExtCommandsHandler::handlerFileStatus(QStringList &args, bool* is_cached) {
    if (args.size() != 1) {
        return ;
    }

    QString file_path = args.first().replace("\\", "/");
    // TODO: delete it
    qWarning("file path is %s", toCStr(file_path));
    *is_cached = isFileCached(file_path);
}

bool ExtCommandsHandler::isFileCached(const QString &path) {
    QString repo_id;
    QString path_in_repo;
    Account account;
    if (!lookUpFileInformation(path, &account, &repo_id, &path_in_repo)) {
        qWarning("[ext] invalid path %s", toCStr(path));
        return false;
    }

    QMutexLocker lock(&rpc_client_mutex_);
    return rpc_client_->isFileCached(repo_id, path_in_repo);
}

bool ExtCommandsHandler::lookUpFileInformation(const QString &path,
                                               Account *account,
                                               QString *ptr_repo_id,
                                               QString *ptr_path_in_repo)
{
    if (!parseRepoFileInfo(path, account, ptr_repo_id, ptr_path_in_repo)) {
        return false;
    }

    return true;
}

// Get thumbanil from server and return the cached thumbnail path
QString ExtCommandsHandler::handlerGetThumbnailFromServer(QStringList& args) {
    if (args.size() != 1) {
        qWarning("invalid command args of get thumbnail");
        return "Failed";
    }

    QString cached_thumbnail_path;
    QString uncached_thumbnail_path = args.first().replace("\\", "/");
    bool success = fetchThumbnail(uncached_thumbnail_path, 256, &cached_thumbnail_path);
    if (!success) {
        qWarning("fetch thumbnail from server failed");
        return "Failed";
    }
    return cached_thumbnail_path;
}

// Get thumbnail from server
bool ExtCommandsHandler::fetchThumbnail(const QString &path, int size, QString *file) {
    QString repo_id;
    QString path_in_repo;
    Account account;
    if (!lookUpFileInformation(path, &account, &repo_id, &path_in_repo)) {
        qWarning("[thumbnailHandler] invalid path %s", toCStr(path));
        return false;
    }

    // TODO: the timeout should be passed by the caller. We use 18s
    // here because the read timeout set by mac ql generator is 20s.
    int timeout_msecs = 18000;
    return ThumbnailService::instance()->getThumbnail(
            account, repo_id, path_in_repo, size, timeout_msecs, file);
}
