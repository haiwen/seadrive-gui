#ifndef SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H
#define SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QUrl>
#include <QString>
class Account;

/**
 * Helper class to handle open local file request
 */
class OpenLocalHelper : public QObject
{
    Q_OBJECT

public:
    struct LocalPath {
        QString account_root;
        QString repo_name;
        QString repo_root;
        QString target_path;
    };

    enum class LocalPathError {
        None,
        RpcUnavailable,
        RepoUnavailable,
        RootUnavailable,
    };

    static OpenLocalHelper* instance();

    // Synchronous, GUI-thread-only path calculation for an already chosen account.
    // Does not open targets or check existence, containment or canonical paths.
    // URL callers must perform their own safety checks. On failure, result is
    // cleared. result must be non-null.
    static LocalPathError resolveLocalPath(const Account& account,
                                           const QString& repo_id,
                                           const QString& path_in_repo,
                                           LocalPath* result);

    void handleOpenLocalFromCommandLine(const QString &url);

    void checkPendingOpenLocalRequest();

public slots:
    // Accepts a valid request for queued processing; does not report opening success.
    bool openLocalFile(const QUrl &url);

private:
    enum class OpenLocalStatus {
        Success,
        NotReady,
        Failed,
    };

    static OpenLocalHelper* singleton_;

    OpenLocalHelper();
    ~OpenLocalHelper();

    OpenLocalStatus findLocalAccount(const QString& repo_id,
                                    Account* account, QString* error);

    OpenLocalStatus validateLocalPath(const LocalPath& local_path,
                                     const QString& path_in_repo,
                                     QString* path_to_open, QString* error);

    OpenLocalStatus openLocalFileByRepo(const QString& repo_id,
                                       const QString& path_in_repo,
                                       QString* error);

    void clearPendingRequest();
    void logRequest(quint64 id, const QString& message, bool warning = false);

    QString pending_repo_id_;
    QString pending_path_;
    QElapsedTimer pending_elapsed_;
    QTimer retry_timer_;
    quint64 request_id_ = 0;
    QString last_wait_reason_;
    bool processing_started_ = false;
    bool processing_ = false;
};


#endif // SEADRIVE_CLIENT_OPEN_LOCAL_FILE_HELPER_H
