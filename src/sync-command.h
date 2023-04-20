#ifndef SEAFILE_GUI_SYNC_COMMAND_H_
#define SEAFILE_GUI_SYNC_COMMAND_H_
#include <QObject>
#include <QString>
#include <cstdint>
#include <vector>
#include "utils/stl.h"
#include "api/api-error.h"

class Account;

class SyncCommand : public QObject {
    Q_OBJECT
public:
    SyncCommand();
    ~SyncCommand();
    void doShareLink(const Account &account, const QString &repo_id, const QString &path);
    void doInternalLink(const Account &account, const QString &repo_id, const QString &path, bool is_dir);
    void doGetUploadLink(const Account &account, const QString &repo_id, const QString& path);
    void doShowFileHistory(const Account &account, const QString &repo_id, const QString& path);
private slots:
    void onShareLinkGenerated(const QString& link);
    void onShareLinkGeneratedFailed(const ApiError& error);
    void onGetSmartLinkSuccess(const QString& smart_link);
    void onGetSmartLinkFailed(const ApiError& error);
    void onGetUploadLinkSuccess(const QString& upload_link);
    void onGetUploadLinkFailed(const ApiError& error);
private:
};

#endif // SEAFILE_GUI_SYNC_COMMAND_H_
