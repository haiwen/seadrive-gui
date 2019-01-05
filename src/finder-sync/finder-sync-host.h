#ifndef SEAFILE_CLIENT_FINDER_SYNC_HOST_H_
#define SEAFILE_CLIENT_FINDER_SYNC_HOST_H_
#include <QObject>
#include <QString>
#include <cstdint>
#include <vector>
#include "utils/stl.h"
#include "api/api-error.h"
#include "api/server-repo.h"

const int kWatchDirMax = 100;
const int kPathMaxSize = 1024;

class Account;
class SeafileRpcClient;
struct ShareLinkInfo;
class FinderSyncHost : public QObject {
    Q_OBJECT
public:
    FinderSyncHost();
    ~FinderSyncHost();
    // called from another thread
    std::string getWatchSet();
    // called from another thread
    uint32_t getFileStatus(const QString& path);
private slots:
    void updateWatchSet();
    void doLockFile(const QString &path, bool lock);
    void doShareLink(const QString &path);
    void doInternalLink(const QString &path);
    void onShareLinkGenerated(const QString& link);
    void onLockFileSuccess();
    void doShowFileHistory(const QString& path);
    void doDownloadFile(const QString& path);
    void onGetSmartLinkSuccess(const QString& smart_link);
    void onGetSmartLinkFailed(const ApiError& error);
    void shareFinderFileDirentSuccess(const ShareLinkInfo& link, const QString& repo_id);
    void shareFinderFileDirentFailed(const ApiError& error);
    void onGetRepoSuccess(const ServerRepo& repo);
    void onGetRepoFailed(const ApiError& error);
private:
    void GetRepo(const QString &path);
    bool lookUpFileInformation(const QString &path, QString *repo_id, QString *path_in_repo);
    SeafileRpcClient *rpc_client_;
    QString path_;
    QString repo_id_;
    QString path_in_repo_;
    bool isUpload_;
};

#endif // SEAFILE_CLIENT_FINDER_SYNC_HOST_H_
