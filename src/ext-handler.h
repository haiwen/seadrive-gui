#ifndef SEADRIVE_CLIENT_EXT_HANLDER_H
#define SEADRIVE_CLIENT_EXT_HANLDER_H

#include <QtGlobal>
#include <QObject>
#include <QThread>
#include <QList>
#include <QHash>

#ifdef Q_OS_WIN32
#include <windows.h>
#else
typedef int HANDLE;
#endif

#include "utils/singleton.h"
#include "account.h"

class SeafileRpcClient;
class ExtConnectionListenerThread;
class ApiError;

/**
 * Handles commands from seafile shell extension
 */
class SeafileExtensionHandler : public QObject {
    SINGLETON_DEFINE(SeafileExtensionHandler)
    Q_OBJECT
public:
    SeafileExtensionHandler();
    void start();
    void stop();

private slots:
    void onDaemonRestarted();
    void onShareLinkGenerated(const QString& link);
    void onShareLinkGeneratedFailed(const ApiError& error);
    void onLockFileSuccess();
    void onLockFileFailed(const ApiError& error);
    void generateShareLink(const Account& account,
                           const QString& repo_id,
                           const QString& path_in_repo,
                           bool is_file,
                           bool internal);
    void lockFile(const Account& account,
                  const QString& repo_id,
                  const QString& path_in_repo,
                  bool lock);
    void privateShare(const Account& account,
                      const QString& repo_id,
                      const QString& path_in_repo,
                      bool to_group);
    void openUrlWithAutoLogin(const Account& account, const QUrl& url);
    void onGetSmartLinkSuccess(const QString& smart_link);
    void onGetSmartLinkFailed(const ApiError& error);
    void getUploadLink(const Account& account, const QString& repo, const QString& path_in_repo);
    void onGetUploadLinkSuccess(const QString &upload_link);
    void onGetUploadLinkFailed(const ApiError& error);

    void showLockedBy(const Account& account, const QString& repo, const QString& path_in_repo);
    void onGetFileLockInfoSuccess(bool found, const QString &owner);
    void onGetFileLockInfoFailed(const ApiError& error);

private:
    ExtConnectionListenerThread *listener_thread_;

    bool started_;
};

/**
 * Creates the named pipe and listen for incoming connections in a separate
 * thread.
 *
 * When a connection is accepted, create a new ExtCommandsHandler thread to
 * serve it.
 */
class ExtConnectionListenerThread : public QThread {
    Q_OBJECT
public:
    void run();

signals:
    void generateShareLink(const Account& account,
                           const QString& repo_id,
                           const QString& path_in_repo,
                           bool is_file,
                           bool internal);
    void lockFile(const Account& account,
                  const QString& repo_id,
                  const QString& path_in_repo,
                  bool lock);
    void privateShare(const Account& account,
                      const QString& repo_id,
                      const QString& path_in_repo,
                      bool to_group);
    void openUrlWithAutoLogin(const Account& account, const QUrl& url);
    void showLockedBy(const Account& account, const QString& repo, const QString& path_in_repo);
    void getUploadLink(const Account& account, const QString& repo_id, const QString& path_in_repo);

private:
    void servePipeInNewThread(HANDLE pipe);
};

/**
 * Serves one extension connection.
 *
 * It's an endless loop of "read request" -> "handle request" -> "send response".
 */
class ExtCommandsHandler: public QThread {
    Q_OBJECT
public:
    enum FileLockStatus {
        NONE = 0,
        LOCKED_BY_OTHER,
        LOCKED_BY_OWNER,
        LOCKED_AUTO,
    };

    ExtCommandsHandler(HANDLE pipe);
    void run();

signals:
    void generateShareLink(const Account& account,
                           const QString& repo_id,
                           const QString& path_in_repo,
                           bool is_file,
                           bool internal);
    void lockFile(const Account& account,
                  const QString& repo_id,
                  const QString& path_in_repo,
                  bool lock);
    void privateShare(const Account& account,
                      const QString& repo_id,
                      const QString& path_in_repo,
                      bool to_group);
    void openUrlWithAutoLogin(const Account& account, const QUrl& url);
    void showLockedBy(const Account& account, const QString& repo, const QString& path_in_repo);
    void getUploadLink(const Account& account, const QString& repo_id, const QString& path_in_repo);

private:
    HANDLE pipe_;

    // QList<QString> listLocalRepos(quint64 ts = 0);
    bool readRequest(QStringList *args);
    bool sendResponse(const QString& resp);

    void handleGenShareLink(const QStringList& args, bool internal);
    QString handleListRepos(const QStringList& args);
#ifdef Q_OS_WIN32
    QString handleGetFileLockStatus(const QStringList& args);
#else
    QString handleGetFileStatus(const QStringList& args);
#endif
    void handleLockFile(const QStringList& args, bool lock);
    void handlePrivateShare(const QStringList& args, bool to_group);
    void handleShowHistory(const QStringList& args);
    void handleDownload(const QStringList& args);
    void handleShowLockedBy(const QStringList& args);
    void handleGetUploadLink(const QStringList& args);

    bool parseRepoFileInfo(const QString& path,
                           Account *account,
                           QString *repo_id,
                           QString *path_in_repo);

    bool isFileCached(const QString &path);
    bool handleIsFileCached(QStringList &args);
    bool isFileInRepo(const QString &path);
    bool handleIsFileInRepo(QStringList &args);
    QString handleShowAccounts();
    void handleDeleteAccount(QStringList args);
    void handleResyncAccount(QStringList args);
    QString handleGetMountPoint();
    QString handleGetThumbnailFromServer(QStringList &args);
    bool fetchThumbnail(const QString &path, int size, QString *file);
};

#endif // SEADRIVE_CLIENT_EXT_HANLDER_H
