#ifndef SEAFILE_CLIENT_RPC_CLIENT_H
#define SEAFILE_CLIENT_RPC_CLIENT_H

#include <vector>

#include <QObject>
#include <QMutex>

extern "C" {

struct _GList;
// Can't forward-declare type SearpcClient here because it is an anonymous typedef struct
struct _SearpcClient;

}

class Account;
class CommitDetails;

class SeafileRpcClient : public QObject {
    Q_OBJECT

public:
    SeafileRpcClient();
    ~SeafileRpcClient();
    void connectDaemon();

    int seafileGetConfig(const QString& key, QString *value);
    int seafileGetConfigInt(const QString& key, int *value);

    int seafileSetConfig(const QString& key, const QString& value);
    int seafileSetConfigInt(const QString& key, int value);

    int getUploadRate(int *rate);
    int getDownloadRate(int *rate);

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

    int getRepoFileStatus(const QString& repo_id,
                          const QString& path_in_repo,
                          bool isdir,
                          QString *status);

    int markFileLockState(const QString& repo_id,
                          const QString& path_in_repo,
                          bool lock);

    bool setServerProperty(const QString &url,
                           const QString &key,
                           const QString &value);

    bool switchAccount(const Account& account);

private:
    Q_DISABLE_COPY(SeafileRpcClient)

    int setRateLimit(bool upload, int limit);


    _SearpcClient *seadrive_rpc_client_;
};

#endif
