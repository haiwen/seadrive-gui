#ifndef SEADRIVE_CLIENT_EXT_HANLDER_H
#define SEADRIVE_CLIENT_EXT_HANLDER_H

#include <QObject>
#include <QThread>
#include <QList>
#include <QHash>

#include <windows.h>

#include "utils/singleton.h"
#include "account.h"

class SeafileRpcClient;
class ExtConnectionListenerThread;
class ApiError;
class CreateShareLinkRequest;

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
    void getShareLinkSuccess(const QString& link);
    void getShareLinkFailed(const QString& repo_id,
                            const QString& path);
    void generateAdvancedShareLink(const QString& password,
                                   quint64 valid_days);
    void onLockFileSuccess();
    void onLockFileFailed(const ApiError& error);
    void generateShareLink(const QString& repo_id,
                           const QString& path_in_repo,
                           bool is_file,
                           bool internal,
                           bool advanced);
    void lockFile(const QString& repo_id,
                  const QString& path_in_repo,
                  bool lock);
    void privateShare(const QString& repo_id,
                      const QString& path_in_repo,
                      bool to_group);
    void openUrlWithAutoLogin(const QUrl& url);

private:
    ExtConnectionListenerThread *listener_thread_;

    bool started_;

    CreateShareLinkRequest *advanced_share_req_;
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
    void generateShareLink(const QString& repo_id,
                           const QString& path_in_repo,
                           bool is_file,
                           bool internal,
                           bool advanced);
    void lockFile(const QString& repo_id,
                  const QString& path_in_repo,
                  bool lock);
    void privateShare(const QString& repo_id,
                      const QString& path_in_repo,
                      bool to_group);
    void openUrlWithAutoLogin(const QUrl& url);

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
    ExtCommandsHandler(HANDLE pipe);
    void run();

signals:
    void generateShareLink(const QString& repo_id,
                           const QString& path_in_repo,
                           bool is_file,
                           bool internal,
                           bool advanced);
    void lockFile(const QString& repo_id,
                  const QString& path_in_repo,
                  bool lock);
    void privateShare(const QString& repo_id,
                      const QString& path_in_repo,
                      bool to_group);
    void openUrlWithAutoLogin(const QUrl& url);

private:
    HANDLE pipe_;

    // QList<QString> listLocalRepos(quint64 ts = 0);
    bool readRequest(QStringList *args);
    bool sendResponse(const QString& resp);

    void handleGenShareLink(const QStringList& args,
                            bool internal,
                            bool advanced);
    QString handleListRepos(const QStringList& args);
    QString handleGetFileStatus(const QStringList& args);
    void handleLockFile(const QStringList& args, bool lock);
    void handlePrivateShare(const QStringList& args, bool to_group);
    void handleShowHistory(const QStringList& args);

    bool parseRepoFileInfo(const QString& path,
                           QString *repo_uname,
                           QString *repo_id,
                           QString *path_in_repo);
};

#endif // SEADRIVE_CLIENT_EXT_HANLDER_H
