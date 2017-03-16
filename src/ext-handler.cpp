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
#include <QScopedPointer>
#include <QList>
#include <QVector>
#include <QDir>
#include <QTimer>
#include <QDateTime>
#include <QDebug>

#include "utils/file-utils.h"
#include "api/requests.h"
#include "ui/sharedlink-dialog.h"
#include "ui/advanced-sharedlink-dialog.h"
#include "ui/seafilelink-dialog.h"
// #include "ui/private-share-dialog.h"
#include "rpc/rpc-client.h"
#include "api/api-error.h"
#include "seadrive-gui.h"
#include "account-mgr.h"
#include "settings-mgr.h"
#include "utils/utils.h"
#include "auto-login-service.h"
#include "ext-handler.h"

namespace {

const char *kSeafExtPipeName = "\\\\.\\pipe\\seadrive_ext_pipe";
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

} // namespace


SINGLETON_IMPL(SeafileExtensionHandler)

static SeafileRpcClient *rpc_client_;
static QMutex rpc_client_mutex_;

SeafileExtensionHandler::SeafileExtensionHandler()
: started_(false)
{
    listener_thread_ = new ExtConnectionListenerThread;

    connect(listener_thread_, SIGNAL(generateShareLink(const QString&, const QString&, bool, bool, bool)),
            this, SLOT(generateShareLink(const QString&, const QString&, bool, bool, bool)));

    connect(listener_thread_, SIGNAL(lockFile(const QString&, const QString&, bool)),
            this, SLOT(lockFile(const QString&, const QString&, bool)));

    connect(listener_thread_, SIGNAL(privateShare(const QString&, const QString&, bool)),
            this, SLOT(privateShare(const QString&, const QString&, bool)));

    connect(listener_thread_, SIGNAL(openUrlWithAutoLogin(const QUrl&)),
            this, SLOT(openUrlWithAutoLogin(const QUrl&)));

    rpc_client_ = new SeafileRpcClient();
}

void SeafileExtensionHandler::start()
{
    rpc_client_->connectDaemon();
    listener_thread_->start();
    started_ = true;
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
                                                bool internal,
                                                bool advanced)
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
        SeafileLinkDialog *dialog = new SeafileLinkDialog(repo_id, account, path, NULL);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    } else if (advanced) {
        AdvancedSharedLinkDialog *dialog = new AdvancedSharedLinkDialog("test", NULL);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    } else {
        GetSharedLinkRequest *req = new GetSharedLinkRequest(
            account, repo_id, path_in_repo);

        connect(req, SIGNAL(success(const QString&)),
                this, SLOT(getShareLinkSuccess(const QString&)));
        connect(req, SIGNAL(failed(const QString&, const QString&)),
                this, SLOT(getShareLinkFailed(const QString&, const QString&)));

        req->send();
    }
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

void SeafileExtensionHandler::getShareLinkSuccess(const QString& link)
{
    SharedLinkDialog *dialog = new SharedLinkDialog(link, NULL);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}

void SeafileExtensionHandler::getShareLinkFailed(const QString& repo_id,
                                                 const QString& path)
{
    const Account account = gui->accountManager()->currentAccount();
    CreatShareLinkRequest *req = new CreatShareLinkRequest(
        account, repo_id, path);

    connect(req, SIGNAL(success(const QString&)),
            this, SLOT(getShareLinkSuccess(const QString&)));

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


void ExtConnectionListenerThread::run()
{
    while (1) {
        HANDLE pipe = INVALID_HANDLE_VALUE;
        bool connected = false;

        pipe = CreateNamedPipe(
            kSeafExtPipeName,         // pipe name
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

    connect(t, SIGNAL(generateShareLink(const QString&, const QString&, bool, bool, bool)),
            this, SIGNAL(generateShareLink(const QString&, const QString&, bool, bool, bool)));
    connect(t, SIGNAL(lockFile(const QString&, const QString&, bool)),
            this, SIGNAL(lockFile(const QString&, const QString&, bool)));
    connect(t, SIGNAL(privateShare(const QString&, const QString&, bool)),
            this, SIGNAL(privateShare(const QString&, const QString&, bool)));
    connect(t, SIGNAL(openUrlWithAutoLogin(const QUrl&)),
            this, SIGNAL(openUrlWithAutoLogin(const QUrl&)));
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
            handleGenShareLink(args, false, false);
        } else if (cmd == "get-advanced-share-link") {
            handleGenShareLink(args, false, true);
        } else if (cmd == "get-internal-link") {
            handleGenShareLink(args, true, false);
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

void ExtCommandsHandler::handleGenShareLink(const QStringList& args,
                                            bool internal,
                                            bool advanced)
{
    if (args.size() != 1) {
        return;
    }

    QString path = args[0];
    QString repo;
    QString path_in_repo = "";
    if (!getRepoAndRelativePath(path, &repo, &path_in_repo)) {
        qWarning() << "failed to getRepoAndRelativePath for " << path;
        return;
    }

    QString repo_id;

    QMutexLocker locker(&rpc_client_mutex_);
    if (!rpc_client_->getRepoIdByPath(repo, &repo_id)) {
        qWarning() << "failed to get the repo id for " << path;
        return;
    }

    bool is_file = QFileInfo(path).isDir();

    emit generateShareLink(repo_id, path_in_repo, is_file, internal, advanced);

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
    foreach (const QString& subdir, subdirs) {
        fullpaths << pathJoin(gui->mountDir(), subdir);
    }

    return fullpaths.join("\n");
}

QString ExtCommandsHandler::handleGetFileStatus(const QStringList& args)
{
    if (args.size() != 1) {
        return "";
    }

    QString path = args[0];
    QString repo;
    QString path_in_repo = "";
    if (!getRepoAndRelativePath(path, &repo, &path_in_repo)) {
        qWarning() << "failed to getRepoAndRelativePath for " << path;
        return "";
    }

    // qWarning() << "handleGetFileStatus: repo = " << repo << " , path_in_repo = " << path_in_repo;

    QString status;

    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_->getRepoFileStatus(repo, path_in_repo, &status) != 0) {
        qWarning("failed to get file status for %s", path_in_repo.toUtf8().data());
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
    QString repo;
    QString path_in_repo = "";
    if (!getRepoAndRelativePath(path, &repo, &path_in_repo)) {
        qWarning() << "failed to getRepoAndRelativePath for " << path;
        return;
    }

    QString repo_id;

    QMutexLocker locker(&rpc_client_mutex_);
    if (!rpc_client_->getRepoIdByPath(repo, &repo_id)) {
        qWarning() << "failed to get the repo id for " << path;
        return;
    }

    if (!rpc_client_->markFileLockState(repo_id, path_in_repo, lock)) {
        qWarning() << "failed to lock file " << path;
        return;
    }
}

bool ExtCommandsHandler::parseRepoFileInfo(const QString& path,
                                           QString *p_repo_uname,
                                           QString *p_repo_id,
                                           QString *p_path_in_repo)
{
    if (!getRepoAndRelativePath(path, p_repo_uname, p_path_in_repo)) {
        qWarning() << "failed to getRepoAndRelativePath for " << path;
        return false;
    }

    QMutexLocker locker(&rpc_client_mutex_);
    if (!rpc_client_->getRepoIdByPath(*p_repo_uname, p_repo_id)) {
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

    QString repo_uname, repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_uname, &repo_id, &path_in_repo)) {
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
    QString repo_uname, repo_id, path_in_repo;
    if (!parseRepoFileInfo(path, &repo_uname, &repo_id, &path_in_repo)) {
        return;
    }

    QUrl url = "/repo/file_revisions/" + repo_id + "/";
    url = ::includeQueryParams(url, {{"p", path_in_repo}});
    emit openUrlWithAutoLogin(url);
}
