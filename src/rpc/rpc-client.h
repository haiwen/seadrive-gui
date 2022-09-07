#ifndef SEAFILE_CLIENT_RPC_CLIENT_H
#define SEAFILE_CLIENT_RPC_CLIENT_H

#include <vector>

#include <QObject>
#include <QMutex>

// Here we can't forward-declare type json_t because it is an anonymous typedef
// struct, and unlike libsearpc we have no way to rewrite its definition to give
// it a name.
#include <jansson.h>

extern "C" {

struct _GList;
// Can't forward-declare type SearpcClient here because it is an anonymous typedef struct
struct _SearpcClient;

}

class Account;
#if defined(_MSC_VER)
class AccountManager;
#endif
class CommitDetails;

class SeafileRpcClient : public QObject {
    Q_OBJECT

public:
    SeafileRpcClient();
    ~SeafileRpcClient();
    void connectDaemon();

    bool isConnected() const { return connected_; }

    int seafileGetConfig(const QString& key, QString *value);
    int seafileGetConfigInt(const QString& key, int *value);

    int seafileSetConfig(const QString& key, const QString& value);
    int seafileSetConfigInt(const QString& key, int value);

    int getUploadRate(int *rate);
    int getDownloadRate(int *rate);

    bool getUploadProgress(json_t **ret_obj);
    bool getDownloadProgress(json_t **ret_obj);

    int getRepoTransferInfo(const QString& repo_id, int *rate, int *percent);

    int setUploadRateLimit(int limit);
    int setDownloadRateLimit(int limit);

    int getRepoProperty(const QString& repo_id,
                        const QString& name,
                        QString *value);

    int setRepoProperty(const QString& repo_id,
                        const QString& name,
                        const QString& value);

    int setRepoToken(const QString &repo_id,
                     const QString& token);

#if defined(Q_OS_WIN32)
    bool getRepoFileLockStatus(const QString& repo_id,
                               const QString& path_in_repo,
                               int* status);
#endif

    int getCategorySyncStatus(const QString& category, QString *status);

    int markFileLockState(const QString& repo_id,
                          const QString& path_in_repo,
                          bool lock);

    bool setServerProperty(const QString &url,
                           const QString &key,
                           const QString &value);

    bool addAccount(const Account& account);

    bool logoutAccount(const Account& account, bool remove_cache);
    bool deleteAccount(const Account& account);

    bool getRepoIdByPath(const QString& server,
                         const QString& username,
                         const QString& repo_uname,
                         QString *repo_id);

    bool getRepoUnameById(const QString& repo_id, QString *repo_uname);

    bool getSyncNotification(json_t **ret);

    bool getGlobalSyncStatus(json_t **ret);

    bool getSeaDriveEvents(json_t **ret_obj);

    bool setCacheCleanIntervalMinutes(int interval);

    bool setCacheSizeLimitGB(int limit);

    bool getCacheCleanIntervalMinutes(int *value);

    bool getCacheSizeLimitGB(int *value);

    bool getSyncErrors(json_t **ret_obj);

    bool cachePath(const QString& repo_id,
                   const QString& path_in_repo);

    bool isFileCached(const QString& repo_id,
                      const QString& path_in_repo);

    bool getEncryptedRepoList(json_t **ret_obj);

    bool setEncryptedRepoPassword(const QString& repo_id,
                                  const QString& password,
                                  QString* error_msg);

    bool clearEncryptedRepoPassword(const QString& repo_id);

    bool exitSeadriveDaemon();

    bool unCachePath(const QString& repo_id,
                     const QString& path_in_repo);

private:
    Q_DISABLE_COPY(SeafileRpcClient)

    int setRateLimit(bool upload, int limit);

    _SearpcClient *seadrive_rpc_client_;

    bool connected_;
};

#endif
