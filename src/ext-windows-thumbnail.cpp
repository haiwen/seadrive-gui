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
#include "rpc/rpc-client.h"
#include "api/api-error.h"
#include "seadrive-gui.h"
#include "daemon-mgr.h"
#include "settings-mgr.h"
#include "utils/utils.h"
#include "utils/utils-win.h"
#include "ext-windows-thumbnail.h"

namespace {

const char *kSeafExtPipeName = "\\\\.\\pipe\\seadrive_ext_windows_thumbail_pipe_";
const int kPipeBufSize = 1024;

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


/**
 * windows thumbnail handler
 */
SINGLETON_IMPL(WindowsThumbnailExtensionHandler)

static SeafileRpcClient *rpc_client_;
static QMutex rpc_client_mutex_;

WindowsThumbnailExtensionHandler::WindowsThumbnailExtensionHandler()
: started_(false)
{
    listener_thread_ = new ExtThumbnailConnectionListenerThread;
    rpc_client_ = new SeafileRpcClient();
}

void WindowsThumbnailExtensionHandler::start()
{
    rpc_client_->connectDaemon();
    listener_thread_->start();
    started_ = true;

    connect(gui->daemonManager(), SIGNAL(daemonRestarted()), this, SLOT(onDaemonRestarted()));
}

void WindowsThumbnailExtensionHandler::onDaemonRestarted()
{
    QMutexLocker locker(&rpc_client_mutex_);
    if (rpc_client_) {
        delete rpc_client_;
    }
    rpc_client_ = new SeafileRpcClient();
    rpc_client_->connectDaemon();
}

// TODO: SHChangeNotify 研究一下扩展中是否用到
void WindowsThumbnailExtensionHandler::stop()
{
    if (started_) {
        // Before seafile client exits, tell the shell to clean all the file
        // status icons
        SHChangeNotify (SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    }
}


/**
 * ExtConnectionListener is Rpc server, it set up a rpc-server for windows thumbnail
 */
void ExtThumbnailConnectionListenerThread::run()
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

void ExtThumbnailConnectionListenerThread::servePipeInNewThread(HANDLE pipe)
{
    ExtThumbnailCommandsHandler *t = new ExtThumbnailCommandsHandler(pipe);
    t->start();
}


/**
 * ExtThumbnailCommandsHandler: is used to handler rpc request for windows thumbnail
 */
ExtThumbnailCommandsHandler::ExtThumbnailCommandsHandler(HANDLE pipe)
{
    pipe_ = pipe;
}

void ExtThumbnailCommandsHandler::run()
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
        if (cmd == "get-cached-status") {
            resp = handlerFileStatus(args.takeAt(1));
        } else if (cmd == "get-disk-letter") {
            QString diskletter;
            if (getDiskLetter(&diskletter)) {
                resp = toCStr(diskletter);
            }
        }
         else {
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

bool ExtThumbnailCommandsHandler::readRequest(QStringList *args)
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

bool ExtThumbnailCommandsHandler::sendResponse(const QString& resp)
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

bool ExtThumbnailCommandsHandler::handlerFileStatus(const QString &path) {
    return isFileCached(path);
}

bool ExtThumbnailCommandsHandler::isFileCached(const QString &path) {
    QString repo_id;
    QString path_in_repo;
    if (!lookUpFileInformation(path, &repo_id, &path_in_repo)) {
        qWarning("[ExtThumbnailCommandsHandler] invalid path %s", toCStr(path));
        return false;
    }

    QMutexLocker lock(&rpc_client_mutex_);
    return rpc_client_->isFileCached(repo_id, path_in_repo);
}

bool ExtThumbnailCommandsHandler::lookUpFileInformation(const QString &path,
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

bool ExtThumbnailCommandsHandler::getDiskLetter(QString disk_letter)
{
    if (!gui->settingsManager()->getDiskLetter(&disk_letter)) {
        qWarning("get disk letter failed");
        return false;
    }
    return true;
}