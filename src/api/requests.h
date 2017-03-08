#ifndef SEAFILE_CLIENT_API_REQUESTS_H
#define SEAFILE_CLIENT_API_REQUESTS_H

#include <QMap>
#include <vector>

#include "account.h"
#include "api-request.h"
#include "contact-share-info.h"
#include "server-repo.h"
#include "server-repo.h"

class QDir;
class QNetworkReply;
class QImage;
class QStringList;

class ServerRepo;
class Account;
class StarredFile;
class SeafEvent;
class CommitDetails;
class SeafDirent;

class PingServerRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    PingServerRequest(const QUrl& serverAddr);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success();

private:
    Q_DISABLE_COPY(PingServerRequest)
};

class LoginRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    LoginRequest(const QUrl& serverAddr,
                 const QString& username,
                 const QString& password,
                 const QString& computer_name);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(const QString& token);

private:
    Q_DISABLE_COPY(LoginRequest)
};


class ListReposRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    explicit ListReposRequest(const Account& account);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(const std::vector<ServerRepo>& repos);

private:
    Q_DISABLE_COPY(ListReposRequest)
};


class RepoDownloadInfo
{
public:
    int repo_version;
    QString relay_id;
    QString relay_addr;
    QString relay_port;
    QString email;
    QString token;
    QString repo_id;
    QString repo_name;
    bool encrypted;
    bool readonly;
    int enc_version;
    QString magic;
    QString random_key;
    QString more_info;

    static RepoDownloadInfo fromDict(QMap<QString, QVariant>& dict,
                                     const QUrl& url,
                                     bool read_only);
};

class DownloadRepoRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    explicit DownloadRepoRequest(const Account& account,
                                 const QString& repo_id,
                                 bool read_only);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(const RepoDownloadInfo& info);

private:
    Q_DISABLE_COPY(DownloadRepoRequest)

    bool read_only_;
};

class GetRepoRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    explicit GetRepoRequest(const Account& account, const QString& repoid);
    const QString& repoid()
    {
        return repoid_;
    }

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(const ServerRepo& repo);

private:
    Q_DISABLE_COPY(GetRepoRequest)
    const QString repoid_;
};

class CreateRepoRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    CreateRepoRequest(const Account& account,
                      const QString& name,
                      const QString& desc,
                      const QString& passwd);
    CreateRepoRequest(const Account& account,
                      const QString& name,
                      const QString& desc,
                      int enc_version,
                      const QString& repo_id,
                      const QString& magic,
                      const QString& random_key);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(const RepoDownloadInfo& info);

private:
    Q_DISABLE_COPY(CreateRepoRequest)
};

class CreateSubrepoRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    explicit CreateSubrepoRequest(const Account& account,
                                  const QString& name,
                                  const QString& repoid,
                                  const QString& path,
                                  const QString& passwd);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(const QString& sub_repoid);

private:
    Q_DISABLE_COPY(CreateSubrepoRequest)
};

class GetUnseenSeahubNotificationsRequest : public SeafileApiRequest
{
    Q_OBJECT

public:
    explicit GetUnseenSeahubNotificationsRequest(const Account& account);

protected slots:
    void requestSuccess(QNetworkReply& reply);

signals:
    void success(int count);

private:
    Q_DISABLE_COPY(GetUnseenSeahubNotificationsRequest)
};

class GetDefaultRepoRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetDefaultRepoRequest(const Account& account);

signals:
    void success(bool exists, const QString& repo_id);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetDefaultRepoRequest);
};

class CreateDefaultRepoRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    CreateDefaultRepoRequest(const Account& account);

signals:
    void success(const QString& repo_id);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(CreateDefaultRepoRequest);
};

class GetLatestVersionRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetLatestVersionRequest(const QString& client_id);

signals:
    void success(const QString& latest_version);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetLatestVersionRequest);
};

class GetCommitDetailsRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetCommitDetailsRequest(const Account& account,
                            const QString& repo_id,
                            const QString& commit_id);

signals:
    void success(const CommitDetails& result);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetCommitDetailsRequest);
};

class FetchImageRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    FetchImageRequest(const QString& img_url);

signals:
    void success(const QImage& avatar);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(FetchImageRequest);
};

class GetAvatarRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetAvatarRequest(const Account& account,
                     const QString& email,
                     qint64 mtime,
                     int size);

    ~GetAvatarRequest();

    const QString& email() const
    {
        return email_;
    }
    const Account& account() const
    {
        return account_;
    }
    qint64 mtime() const
    {
        return mtime_;
    }

signals:
    void success(const QImage& avatar);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetAvatarRequest);

    FetchImageRequest* fetch_img_req_;

    QString email_;

    Account account_;

    qint64 mtime_;
};

class SetRepoPasswordRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    SetRepoPasswordRequest(const Account& account,
                           const QString& repo_id,
                           const QString& password);

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(SetRepoPasswordRequest);
};

class ServerInfoRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    ServerInfoRequest(const Account& account);

signals:
    void success(const Account& account, const ServerInfo& info);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(ServerInfoRequest);
    const Account& account_;
};

class LogoutDeviceRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    LogoutDeviceRequest(const Account& account, bool remove_cache);

    const Account& account() const
    {
        return account_;
    }

    bool shouldRemoveCache() const { return remove_cache_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(LogoutDeviceRequest);

    Account account_;

    bool remove_cache_;
};

class GetRepoTokensRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetRepoTokensRequest(const Account& account, const QStringList& repo_ids);

    const QMap<QString, QString>& repoTokens()
    {
        return repo_tokens_;
    }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetRepoTokensRequest);

    QMap<QString, QString> repo_tokens_;
};

class GetLoginTokenRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetLoginTokenRequest(const Account& account, const QString& next_url);

    const Account& account()
    {
        return account_;
    }
    const QString& nextUrl()
    {
        return next_url_;
    }

signals:
    void success(const QString& token);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetLoginTokenRequest);

    Account account_;
    QString next_url_;
};

struct FileSearchResult {
    QString repo_id;
    QString repo_name;
    QString name;
    QString oid;
    qint64 last_modified;
    QString fullpath;
    qint64 size;
};

Q_DECLARE_METATYPE(FileSearchResult)

class FileSearchRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    FileSearchRequest(const Account& account,
                      const QString& keyword,
                      int per_page = 10);
    const QString& keyword() const
    {
        return keyword_;
    }

signals:
    void success(const std::vector<FileSearchResult>& result);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(FileSearchRequest);

    const QString keyword_;
};

class FetchCustomLogoRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    FetchCustomLogoRequest(const QUrl& url);

signals:
    void success(const QUrl& url);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(FetchCustomLogoRequest);
};

class FetchAccountInfoRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    FetchAccountInfoRequest(const Account& account);

    const Account& account() const
    {
        return account_;
    }

signals:
    void success(const AccountInfo& info);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(FetchAccountInfoRequest);

    Account account_;
};

class PrivateShareRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    enum ShareOperation {
        ADD_SHARE,
        UPDATE_SHARE,
        REMOVE_SHARE,
    };
    PrivateShareRequest(const Account& account,
                        const QString& repo_id,
                        const QString& path,
                        const QString& username,
                        int group_id,
                        SharePermission permission,
                        ShareType share_type,
                        ShareOperation op);

    ShareOperation shareOperation() const
    {
        return share_operation_;
    }

    int groupId() const
    {
        return share_type_ == SHARE_TO_GROUP ? group_id_ : -1;
    };

    QString userName() const
    {
        return share_type_ == SHARE_TO_USER ? username_ : QString();
    };

    SharePermission permission() const
    {
        return permission_;
    }

    ShareType shareType() const
    {
        return share_type_;
    }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(PrivateShareRequest);

    int group_id_;
    QString username_;
    SharePermission permission_;
    ShareType share_type_;
    ShareOperation share_operation_;
};

class GetPrivateShareItemsRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    GetPrivateShareItemsRequest(const Account& account,
                                const QString& repo_id,
                                const QString& path);

signals:
    void success(const QList<GroupShareInfo>&, const QList<UserShareInfo>&);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetPrivateShareItemsRequest);
};

class FetchGroupsAndContactsRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    FetchGroupsAndContactsRequest(const Account& account);

signals:
    void success(const QList<SeafileGroup>&, const QList<SeafileUser>&);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(FetchGroupsAndContactsRequest);
};


class RemoteWipeReportRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    RemoteWipeReportRequest(const Account& account);

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(RemoteWipeReportRequest);
};

class SearchUsersRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    SearchUsersRequest(const Account& account, const QString& pattern);

    QString pattern() const { return pattern_; }

signals:
    void success(const QList<SeafileUser>& users);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(SearchUsersRequest);

    QString pattern_;
};

class FetchGroupsRequest : public SeafileApiRequest
{
    Q_OBJECT
public:
    FetchGroupsRequest(const Account& account);

signals:
    void success(const QList<SeafileGroup>&);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(FetchGroupsRequest);
};

class GetDirentsRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    GetDirentsRequest(const Account& account,
                      const QString& repo_id,
                      const QString& path);

    const QString& repoId() const { return repo_id_; }
    const QString& path() const { return path_; }

signals:
    void success(bool current_readonly, const QList<SeafDirent> &dirents);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetDirentsRequest)

    const QString repo_id_;
    const QString path_;
    bool readonly_;
};

class GetFileDownloadLinkRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    GetFileDownloadLinkRequest(const Account &account,
                               const QString &repo_id,
                               const QString &path);

    QString fileId() const { return file_id_; }
signals:
    void success(const QString& url);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetFileDownloadLinkRequest)

    QString file_id_;
};

// TODO:
// intergrate file creation into this class
class CreateDirectoryRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    CreateDirectoryRequest(const Account &account, const QString &repo_id,
                           const QString &path, bool create_parents = false);
    const QString &repoId() { return repo_id_; }
    const QString &path() { return path_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(CreateDirectoryRequest)
    const QString repo_id_;
    const QString path_;
    bool create_parents_;
};

class RenameDirentRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    RenameDirentRequest(const Account &account, const QString &repo_id,
                        const QString &path, const QString &new_path,
                        bool is_file = true);

    const bool& isFile() const { return is_file_; }
    const QString& repoId() const { return repo_id_; }
    const QString& path() const { return path_; }
    const QString& newName() const { return new_name_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(RenameDirentRequest)

    const bool is_file_;
    const QString repo_id_;
    const QString path_;
    const QString new_name_;
};

class RemoveDirentRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    RemoveDirentRequest(const Account &account, const QString &repo_id,
                        const QString &path, bool is_file = true);

    const bool& isFile() const { return is_file_; }
    const QString& repoId() const { return repo_id_; }
    const QString& path() const { return path_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(RemoveDirentRequest)

    const bool is_file_;
    const QString repo_id_;
    const QString path_;
};

struct SharedLinkResult {
    qint64 ctime;
    QString expire_date;
    bool is_dir;
    bool is_expired;
    QString link;
    QString obj_name;
    QString path;
    QString repo_id;
    QString repo_name;
    QString token;
    QString username;
    quint64 view_cnt;
};

class GetSharedLinkRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    GetSharedLinkRequest(const Account &account,
                         const QString &repo_id,
                         const QString &path);

signals:
    void success(const QString& url);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetSharedLinkRequest)
};

class GetFileUploadLinkRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    GetFileUploadLinkRequest(const Account &account,
                             const QString &repo_id,
                             const QString &path,
                             bool use_upload = true);

signals:
    void success(const QString& url);

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(GetFileUploadLinkRequest)
};

// Single File only
class MoveFileRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    MoveFileRequest(const Account &account,
                    const QString &repo_id,
                    const QString &path,
                    const QString &dst_repo_id,
                    const QString &dst_dir_path);

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(MoveFileRequest)
};

class CopyMultipleFilesRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    CopyMultipleFilesRequest(const Account &account,
                             const QString &repo_id,
                             const QString &src_dir_path,
                             const QStringList &src_file_names,
                             const QString &dst_repo_id,
                             const QString &dst_dir_path);
    const QString& repoId() { return repo_id_; }
    const QString& srcPath() { return src_dir_path_; }
    const QStringList& srcFileNames() { return src_file_names_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(CopyMultipleFilesRequest)
    const QString repo_id_;
    const QString src_dir_path_;
    const QStringList src_file_names_;
};

class MoveMultipleFilesRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    MoveMultipleFilesRequest(const Account &account,
                             const QString &repo_id,
                             const QString &src_dir_path,
                             const QStringList &src_file_names,
                             const QString &dst_repo_id,
                             const QString &dst_dir_path);
    const QString& srcRepoId() { return repo_id_; }
    const QString& srcPath() { return src_dir_path_; }
    const QStringList& srcFileNames() { return src_file_names_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(MoveMultipleFilesRequest)
    const QString repo_id_;
    const QString src_dir_path_;
    const QStringList src_file_names_;
};

class StarFileRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    StarFileRequest(const Account &account, const QString &repo_id,
                    const QString &path);

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(StarFileRequest)
};

class UnstarFileRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    UnstarFileRequest(const Account &account, const QString &repo_id,
                      const QString &path);

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(UnstarFileRequest)
};

class LockFileRequest : public SeafileApiRequest {
    Q_OBJECT
public:
    LockFileRequest(const Account& account,
                    const QString& repo_id,
                    const QString& path,
                    bool lock);

    bool lock() const { return lock_; }
    const QString & repoId() const { return repo_id_; }
    const QString & path() const { return path_; }

signals:
    void success();

protected slots:
    void requestSuccess(QNetworkReply& reply);

private:
    Q_DISABLE_COPY(LockFileRequest);
    const bool lock_;
    const QString repo_id_;
    const QString path_;
};

class AuthPingRequest: public SeafileApiRequest
{
    Q_OBJECT
public:
    explicit AuthPingRequest(const Account& account);
protected slots:
    void requestSuccess(QNetworkReply& reply);
signals:
    void success();

private:
    Q_DISABLE_COPY(AuthPingRequest)
};


#endif // SEAFILE_CLIENT_API_REQUESTS_H
