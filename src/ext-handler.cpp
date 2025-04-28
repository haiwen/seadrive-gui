#include <fcntl.h>
#include <ctype.h>

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

#ifdef Q_OS_WIN32
#include <winsock2.h>
#include <windows.h>
#include <io.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <userenv.h>

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
#else
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <string.h>

#include <glib.h>

namespace {

const char *kSeafExtPipeName = "seadrive_ext.sock";

bool
extPipeReadN (int pipe_fd, void *vptr, size_t n)
{
    size_t  nleft;
    gssize nread;
    char    *ptr;

    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(pipe_fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return false;
        } else if (nread == 0)
            break;              /* EOF */

        nleft -= nread;
        ptr   += nread;
    }
    return(n - nleft) >= 0;
}

bool
extPipeWriteN(int pipe_fd, void *vptr, size_t n)
{
    size_t      nleft;
    gssize     nwritten;
    const char  *ptr;

    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(pipe_fd, ptr, nleft)) <= 0)
        {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;       /* and call write() again */
            else
                return false;         /* error */
        }

        nleft -= nwritten;
        ptr   += nwritten;
    }
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
    return strerror (errno);
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

    QDir sync_root_base = QDir::home().filePath("SeaDrive");
    QString base_dir = sync_root_base.path() + '/';

    if (path.startsWith(base_dir)) {
        relative_path = path.mid(base_dir.length());
    } else {
        return false;
    }

    if (relative_path.isEmpty()){
        return false;
    }

    QStringList parts = relative_path.split('/', Qt::SkipEmptyParts);
    QString display_name = parts[0];

    bool found = false;
    auto accounts = gui->accountManager()->activeAccounts();
    for (auto a : accounts) {
        if (a.displayName == display_name) {
            *account = a;
            // skip display name
            relative_path = relative_path.mid(display_name.length() + 1);
            found = true;
            break;
        }
    }

    if (relative_path.isEmpty()){
        return false;
    }

    if (!found && accounts.size() == 1) {
        *account = accounts.first();
    } else if (!found) {
        return false;
    }

    if (relative_path.endsWith("/")) {
        relative_path = relative_path.left(relative_path.length() - 1);
    }

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
#endif


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

    rpc_client_ = new SeafileRpcClient(EMPTY_DOMAIN_ID);
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
    rpc_client_ = new SeafileRpcClient(EMPTY_DOMAIN_ID);
    rpc_client_->connectDaemon();
}

void SeafileExtensionHandler::stop()
{
#ifdef Q_OS_WIN32
    if (started_) {
        // Before seafile client exits, tell the shell to clean all the file
        // status icons
        SHChangeNotify (SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
#endif
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
    GetUploadLinkRequest *req = qobject_cast<GetUploadLinkRequest *>(sender());
    UploadLinkDialog *dialog = new UploadLinkDialog(upload_link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
    req->deleteLater();
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
    GetSmartLinkRequest *req = (GetSmartLinkRequest *)(sender());
    qWarning("get smart_link failed %s\n", error.toString().toUtf8().data());

    int http_error_code =  error.httpErrorCode();
    if (http_error_code == 403) {
        gui->warningBox(tr("No permissions to create a shared link"));
    } else {
        gui->warningBox(tr("failed get internal link %1").arg(error.toString()));
    }
    req->deleteLater();
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
    req->deleteLater();
}

void SeafileExtensionHandler::onShareLinkGeneratedFailed(const ApiError& error) {
    GetSharedLinkRequest *req = qobject_cast<GetSharedLinkRequest *>(sender());
    int http_error_code = error.httpErrorCode();
    if (http_error_code == 403) {
        gui->warningBox(tr("No permissions to create a shared link"));
    } else {
        gui->messageBox(tr("Failed to get share link %1\n").arg(error.toString()));
    }
    req->deleteLater();
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
    req->deleteLater();
}

void SeafileExtensionHandler::onLockFileFailed(const ApiError& error)
{
    LockFileRequest *req = qobject_cast<LockFileRequest *>(sender());
    QString str = req->lock() ? tr("Failed to lock file") : tr("Failed to unlock file");
    gui->warningBox(QString("%1: %2").arg(str, error.toString()));
    req->deleteLater();
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

#ifdef Q_OS_WIN32
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
#else
void ExtConnectionListenerThread::run()
{
    QString data_dir = seadriveInternalDir();
    QString socket_path = pathJoin(data_dir, kSeafExtPipeName);
    const char *un_path = toCStr(socket_path);
    qWarning("[ext] listening on %s", un_path);
    int pipe_fd = -1;

    pipe_fd = socket (AF_UNIX, SOCK_STREAM, 0);
    if (pipe_fd < 0) {
        qWarning("[ext] Failed to create unix socket fd: %s\n", strerror(errno));
        close (pipe_fd);
        return;
    }

    struct sockaddr_un saddr;
    saddr.sun_family = AF_UNIX;

    if (strlen(un_path) > sizeof(saddr.sun_path)-1) {
        qWarning("[ext] Unix socket path %s is too long."
                 "Please set or modify UNIX_SOCKET option in ccnet.conf.\n",
                 un_path);
        close (pipe_fd);
        return;
    }

    if (g_file_test (un_path, G_FILE_TEST_EXISTS)) {
        qWarning("[ext] socket file exists, delete it anyway\n");
        if (unlink (un_path) < 0) {
            qWarning("[ext] delete ext socket file failed : %s\n", strerror(errno));
            close(pipe_fd);
            return;
        }
    }

    g_strlcpy (saddr.sun_path, un_path, sizeof(saddr.sun_path));
    if (bind(pipe_fd, (struct sockaddr *)&saddr, sizeof(saddr)) < 0) {
        qWarning("[ext] Failed to bind unix socket fd to %s : %s\n",
                un_path, strerror(errno));
        close(pipe_fd);
        return;
    }

    if (listen(pipe_fd, 10) < 0) {
        qWarning("[ext] Failed to listen to unix socket: %s\n", strerror(errno));
        close (pipe_fd);
        return;
    }

    if (chmod(un_path, 0700) < 0) {
        qWarning("[ext] Failed to set permisson for unix socket %s: %s\n",
                 un_path, strerror(errno));
        close (pipe_fd);
        return;
    }

    while (1) {
        int connfd = accept (pipe_fd, NULL, 0);
        if (connfd < 0) {
            qWarning("[ext] Failed to accept from unix socket: %s\n", strerror(errno));
            close (pipe_fd);
            return;
        }

        qDebug ("[ext] Accepted an extension pipe client\n");
        servePipeInNewThread(connfd);
    }
}
#endif

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
#ifdef Q_OS_WIN32
            resp = handleGetFileLockStatus(args);
#else
            resp = handleGetFileStatus(args);
#endif
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
        } else if (cmd == "is-file-cached") {
            bool is_cached = handleIsFileCached(args);
            resp = is_cached ? "cached" : "uncached";
        } else if (cmd == "is-file-in-repo") {
            bool is_in_repo = handleIsFileInRepo(args);
            resp = is_in_repo ? "true" : "false";
        }
#ifdef Q_OS_WIN32
        else if (cmd == "get-thumbnail-from-server") {
            resp = handleGetThumbnailFromServer(args);
        }
#endif
        else {
            qWarning ("[ext] unknown request command: %s", cmd.toUtf8().data());
        }

        if (!sendResponse(resp)) {
            qWarning ("failed to write response to shell extension: %s",
                      formatErrorMessage().c_str());
            break;
        }
    }

#ifdef Q_OS_WIN32
    qDebug ("An extension client is disconnected: GLE=%lu\n",
            GetLastError());
    DisconnectNamedPipe(pipe_);
    CloseHandle(pipe_);
#else
    qDebug ("An extension client is disconnected: %s\n",
            strerror(errno));
    close (pipe_);
#endif
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
#ifdef Q_OS_WIN32
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
#else
    return "";
#endif
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

#ifdef Q_OS_WIN32
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
#else
QString ExtCommandsHandler::handleGetFileStatus(const QStringList& args)
{
    if (args.size() != 1) {
        return "";
    }
    QString path = normalizedPath(args[0]);

    QString status;
    Account account;
    QString path_in_repo;
    QString repo;
    QString category;

    if (!parseFilePath(path, &account, &repo, &path_in_repo, &category)) {
        qWarning() << "failed to parse file path:" << path;
        return "";
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_->getRepoFileStatus(account, path_concat(category, repo), path_in_repo, &status) != 0) {
        return "";
    }

    return status;
}
#endif

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


bool ExtCommandsHandler::handleIsFileCached(QStringList &args) {
    if (args.size() != 1) {
        return false;
    }

    QString file_path = normalizedPath(args.first());
    return isFileCached(file_path);
}

bool ExtCommandsHandler::isFileCached(const QString &path) {
    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        qWarning("[ext] invalid path %s", toCStr(path));
        return false;
    }

    QMutexLocker lock(&rpc_client_mutex_);
    return rpc_client_->isFileCached(repo_id, path_in_repo);
}

bool ExtCommandsHandler::handleIsFileInRepo(QStringList &args) {
    if (args.size() != 1) {
        return false;
    }

    QString file_path = normalizedPath(args.first());
    return isFileInRepo(file_path);
}

bool ExtCommandsHandler::isFileInRepo(const QString &path) {
    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        return false;
    }

    return true;
}


#ifdef Q_OS_WIN32
// Get thumbanil from server and return the cached thumbnail path
QString ExtCommandsHandler::handleGetThumbnailFromServer(QStringList& args) {
    if (args.size() != 2) {
        qWarning("invalid command args of get thumbnail");
        return "";
    }

    QString cached_thumbnail_path;
    QString path = normalizedPath(args[0]);
    int size = args[1].toInt();
    if (size <= 0) {
        size = 64;
    } else {
        size = (size + 63) & (~63);
    }
    bool success = fetchThumbnail(path, size, &cached_thumbnail_path);
    if (!success) {
        qWarning("fetch thumbnail from server failed");
        return "";
    }
    return cached_thumbnail_path;
}

// Get thumbnail from server
bool ExtCommandsHandler::fetchThumbnail(const QString &path, int size, QString *file) {
    Account account;
    QString repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &account, &repo_id, &path_in_repo)) {
        qWarning("[thumbnailHandler] invalid path %s", toCStr(path));
        return false;
    }

    // set timeout to 300s to get thumbnail.
    int timeout_msecs = 300000;
    return ThumbnailService::instance()->getThumbnail(
            account, repo_id, path_in_repo, size, timeout_msecs, file);
}
#endif
