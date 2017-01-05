#include <jansson.h>

#include <QScopedPointer>
#include <QtNetwork>

#include "account.h"

#include "api-error.h"
#include "commit-details.h"
#include "event.h"
// #include "repo-service.h"
// #include "rpc/rpc-client.h"
#include "seadrive-gui.h"
#include "server-repo.h"
#include "utils/api-utils.h"
#include "utils/json-utils.h"
#include "utils/utils.h"
#include "seaf-dirent.h"

#include "requests.h"

namespace
{
const char* kApiPingUrl = "api2/ping/";
const char* kApiLoginUrl = "api2/auth-token/";
const char* kListReposUrl = "api2/repos/";
const char* kCreateRepoUrl = "api2/repos/";
const char* kGetRepoUrl = "api2/repos/%1/";
const char* kCreateSubrepoUrl = "api2/repos/%1/dir/sub_repo/";
const char* kUnseenMessagesUrl = "api2/unseen_messages/";
const char* kDefaultRepoUrl = "api2/default-repo/";
const char* kCommitDetailsUrl = "api2/repo_history_changes/";
const char* kAvatarUrl = "api2/avatars/user/";
const char* kSetRepoPasswordUrl = "api2/repos/";
const char* kServerInfoUrl = "api2/server-info/";
const char* kLogoutDeviceUrl = "api2/logout-device/";
const char* kGetRepoTokensUrl = "api2/repo-tokens/";
const char* kGetLoginTokenUrl = "api2/client-login/";
const char* kFileSearchUrl = "api2/search/";
const char* kAccountInfoUrl = "api2/account/info/";
const char* kDirSharedItemsUrl = "api2/repos/%1/dir/shared_items/";
const char* kFetchGroupsAndContactsUrl = "api2/groupandcontacts/";
const char* kFetchGroupsUrl = "api2/groups/";
const char* kRemoteWipeReportUrl = "api2/device-wiped/";
const char* kSearchUsersUrl = "api2/search-user/";

const char* kGetDirentsUrl = "api2/repos/%1/dir/";
const char* kGetFilesUrl = "api2/repos/%1/file/";
const char* kGetFileSharedLinkUrl = "api2/repos/%1/file/shared-link/";
const char* kGetFileUploadUrl = "api2/repos/%1/upload-link/";
const char* kGetFileUpdateUrl = "api2/repos/%1/update-link/";
const char* kGetStarredFilesUrl = "api2/starredfiles/";
const char* kFileOperationCopy = "api2/repos/%1/fileops/copy/";
const char* kFileOperationMove = "api2/repos/%1/fileops/move/";
const char* kAuthPingURL = "api2/auth/ping/";

const char* kLatestVersionUrl = "https://seafile.com/api/seadrive-latest/";

// #if defined(Q_OS_WIN32)
// const char* kOsName = "windows";
// #elif defined(Q_OS_LINUX)
// const char* kOsName = "linux";
// #else
// const char* kOsName = "mac";
// #endif

// Use `SEADRIVE_LATEST_VERSION_URL` to set the alternative url for checking
// latest version info when developing. E.g. http://localhost:8001
QString getLatestVersionUrl() {
#if defined(SEADRIVE_GUI_DEBUG)
    QString url_from_env = qgetenv("SEADRIVE_LATEST_VERSION_URL");
    return url_from_env.isEmpty() ? kLatestVersionUrl : url_from_env;
#else
    return kLatestVersionUrl;
#endif
}

} // namespace


PingServerRequest::PingServerRequest(const QUrl& serverAddr)

    : SeafileApiRequest(::urlJoin(serverAddr, kApiPingUrl),
                        SeafileApiRequest::METHOD_GET)
{
}

void PingServerRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

/**
 * LoginRequest
 */
LoginRequest::LoginRequest(const QUrl& serverAddr,
                           const QString& username,
                           const QString& password,
                           const QString& computer_name)

    : SeafileApiRequest(::urlJoin(serverAddr, kApiLoginUrl),
                        SeafileApiRequest::METHOD_POST)
{
    setFormParam("username", username);
    setFormParam("password", password);

    QHash<QString, QString> params = ::getSeafileLoginParams(computer_name);
    foreach (const QString& key, params.keys()) {
        setFormParam(key, params[key]);
    }
}

void LoginRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    const char* token =
        json_string_value(json_object_get(json.data(), "token"));
    if (token == NULL) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    emit success(token);
}


/**
 * ListReposRequest
 */
ListReposRequest::ListReposRequest(const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kListReposUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
}

void ListReposRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("ListReposRequest:failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    std::vector<ServerRepo> repos =
        ServerRepo::listFromJSON(json.data(), &error);
    emit success(repos);
}


/**
 * DownloadRepoRequest
 */
DownloadRepoRequest::DownloadRepoRequest(const Account& account,
                                         const QString& repo_id,
                                         bool read_only)
    : SeafileApiRequest(
          account.getAbsoluteUrl("api2/repos/" + repo_id + "/download-info/"),
          SeafileApiRequest::METHOD_GET,
          account.token),
      read_only_(read_only)
{
}

RepoDownloadInfo RepoDownloadInfo::fromDict(QMap<QString, QVariant>& dict,
                                            const QUrl& url_in,
                                            bool read_only)
{
    RepoDownloadInfo info;
    info.repo_version = dict["repo_version"].toInt();
    info.relay_id = dict["relay_id"].toString();
    info.relay_addr = dict["relay_addr"].toString();
    info.relay_port = dict["relay_port"].toString();
    info.email = dict["email"].toString();
    info.token = dict["token"].toString();
    info.repo_id = dict["repo_id"].toString();
    info.repo_name = dict["repo_name"].toString();
    info.encrypted = dict["encrypted"].toInt();
    info.magic = dict["magic"].toString();
    info.random_key = dict["random_key"].toString();
    info.enc_version = dict.value("enc_version", 1).toInt();
    info.readonly = read_only;

    QUrl url = url_in;
    url.setPath("/");
    info.relay_addr = url.host();

    QMap<QString, QVariant> map;
    map.insert("is_readonly", read_only ? 1 : 0);
    map.insert("server_url", url.toString());

    info.more_info = ::mapToJson(map);

    return info;
}

void DownloadRepoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);
    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    RepoDownloadInfo info = RepoDownloadInfo::fromDict(dict, url(), read_only_);

    emit success(info);
}

/**
 * GetRepoRequest
 */
GetRepoRequest::GetRepoRequest(const Account& account, const QString& repoid)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kGetRepoUrl).arg(repoid)),
          SeafileApiRequest::METHOD_GET,
          account.token),
      repoid_(repoid)
{
}

void GetRepoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);
    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);
    ServerRepo repo = ServerRepo::fromJSON(root, &error);

    emit success(repo);
}

/**
 * CreateRepoRequest
 */
CreateRepoRequest::CreateRepoRequest(const Account& account,
                                     const QString& name,
                                     const QString& desc,
                                     const QString& passwd)
    : SeafileApiRequest(account.getAbsoluteUrl(kCreateRepoUrl),
                        SeafileApiRequest::METHOD_POST,
                        account.token)
{
    setFormParam(QString("name"), name);
    setFormParam(QString("desc"), desc);
    if (!passwd.isNull()) {
        qWarning("Encrypted repo");
        setFormParam(QString("passwd"), passwd);
    }
}

CreateRepoRequest::CreateRepoRequest(const Account& account,
                                     const QString& name,
                                     const QString& desc,
                                     int enc_version,
                                     const QString& repo_id,
                                     const QString& magic,
                                     const QString& random_key)
    : SeafileApiRequest(account.getAbsoluteUrl(kCreateRepoUrl),
                        SeafileApiRequest::METHOD_POST,
                        account.token)
{
    setFormParam("name", name);
    setFormParam("desc", desc);
    setFormParam("enc_version", QString::number(enc_version));
    setFormParam("repo_id", repo_id);
    setFormParam("magic", magic);
    setFormParam("random_key", random_key);
}

void CreateRepoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);
    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);
    RepoDownloadInfo info = RepoDownloadInfo::fromDict(dict, url(), false);

    emit success(info);
}

/**
 * CreateSubrepoRequest
 */
CreateSubrepoRequest::CreateSubrepoRequest(const Account& account,
                                           const QString& name,
                                           const QString& repoid,
                                           const QString& path,
                                           const QString& passwd)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kCreateSubrepoUrl).arg(repoid)),
          SeafileApiRequest::METHOD_GET,
          account.token)
{
    setUrlParam(QString("p"), path);
    setUrlParam(QString("name"), name);
    if (!passwd.isNull()) {
        setUrlParam(QString("password"), passwd);
    }
}

void CreateSubrepoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);
    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    emit success(dict["sub_repo_id"].toString());
}

/**
 * GetUnseenSeahubNotificationsRequest
 */
GetUnseenSeahubNotificationsRequest::GetUnseenSeahubNotificationsRequest(
    const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kUnseenMessagesUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
}

void GetUnseenSeahubNotificationsRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning(
            "GetUnseenSeahubNotificationsRequest: failed to parse json:%s\n",
            error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> ret = mapFromJSON(root, &error);

    if (!ret.contains("count")) {
        emit failed(ApiError::fromJsonError());
        return;
    }

    int count = ret.value("count").toInt();
    emit success(count);
}

GetDefaultRepoRequest::GetDefaultRepoRequest(const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kDefaultRepoUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
}

void GetDefaultRepoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("CreateDefaultRepoRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    if (!dict.contains("exists")) {
        emit failed(ApiError::fromJsonError());
        return;
    }

    bool exists = dict.value("exists").toBool();
    if (!exists) {
        emit success(false, "");
        return;
    }

    if (!dict.contains("repo_id")) {
        emit failed(ApiError::fromJsonError());
        return;
    }

    QString repo_id = dict.value("repo_id").toString();

    emit success(true, repo_id);
}


CreateDefaultRepoRequest::CreateDefaultRepoRequest(const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kDefaultRepoUrl),
                        SeafileApiRequest::METHOD_POST,
                        account.token)
{
}

void CreateDefaultRepoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("CreateDefaultRepoRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    if (!dict.contains("repo_id")) {
        emit failed(ApiError::fromJsonError());
        return;
    }

    emit success(dict.value("repo_id").toString());
}

GetLatestVersionRequest::GetLatestVersionRequest(const QString& client_id)
    : SeafileApiRequest(QUrl(getLatestVersionUrl()), SeafileApiRequest::METHOD_GET)
{
    setUrlParam("id", client_id.left(8));
}

void GetLatestVersionRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("GetLatestVersionRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    if (dict.contains("version")) {
        QString version = dict.value("version").toString();
        qWarning("The latest version is %s", toCStr(version));
        emit success(version);
        return;
    }

    emit failed(ApiError::fromJsonError());
}


GetCommitDetailsRequest::GetCommitDetailsRequest(const Account& account,
                                                 const QString& repo_id,
                                                 const QString& commit_id)
    : SeafileApiRequest(
          account.getAbsoluteUrl(kCommitDetailsUrl + repo_id + "/"),
          SeafileApiRequest::METHOD_GET,
          account.token)
{
    setUrlParam("commit_id", commit_id);
}

void GetCommitDetailsRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("GetCommitDetailsRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    CommitDetails details = CommitDetails::fromJSON(json.data(), &error);

    emit success(details);
}

// /api2/user/foo@foo.com/resized/36
GetAvatarRequest::GetAvatarRequest(const Account& account,
                                   const QString& email,
                                   qint64 mtime,
                                   int size)
    : SeafileApiRequest(
          account.getAbsoluteUrl(kAvatarUrl + email + "/resized/" +
                                 QString::number(size) + "/"),
          SeafileApiRequest::METHOD_GET,
          account.token),
      fetch_img_req_(NULL),
      mtime_(mtime)
{
    account_ = account;
    email_ = email;
}

GetAvatarRequest::~GetAvatarRequest()
{
    if (fetch_img_req_) {
        delete fetch_img_req_;
    }
}

void GetAvatarRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("GetAvatarRequest: failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    const char* avatar_url =
        json_string_value(json_object_get(json.data(), "url"));

    // we don't need to fetch all images if we have latest one
    json_t* mtime = json_object_get(json.data(), "mtime");
    if (!mtime) {
        qWarning("GetAvatarRequest: no 'mtime' value in response\n");
    }
    else {
        qint64 new_mtime = json_integer_value(mtime);
        if (new_mtime == mtime_) {
            emit success(QImage());
            return;
        }
        mtime_ = new_mtime;
    }

    if (!avatar_url) {
        qWarning("GetAvatarRequest: no 'url' value in response\n");
        emit failed(ApiError::fromJsonError());
        return;
    }

    QString url = QUrl::fromPercentEncoding(avatar_url);

    fetch_img_req_ = new FetchImageRequest(url);

    connect(fetch_img_req_, SIGNAL(failed(const ApiError&)), this,
            SIGNAL(failed(const ApiError&)));
    connect(fetch_img_req_, SIGNAL(success(const QImage&)), this,
            SIGNAL(success(const QImage&)));
    fetch_img_req_->send();
}

FetchImageRequest::FetchImageRequest(const QString& img_url)
    : SeafileApiRequest(QUrl(img_url), SeafileApiRequest::METHOD_GET)
{
}

void FetchImageRequest::requestSuccess(QNetworkReply& reply)
{
    QImage img;
    img.loadFromData(reply.readAll());

    if (img.isNull()) {
        qWarning("FetchImageRequest: invalid image data\n");
        emit failed(ApiError::fromHttpError(400));
    }
    else {
        emit success(img);
    }
}

SetRepoPasswordRequest::SetRepoPasswordRequest(const Account& account,
                                               const QString& repo_id,
                                               const QString& password)
    : SeafileApiRequest(
          account.getAbsoluteUrl(kSetRepoPasswordUrl + repo_id + "/"),
          SeafileApiRequest::METHOD_POST,
          account.token)
{
    setFormParam("password", password);
}

void SetRepoPasswordRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

ServerInfoRequest::ServerInfoRequest(const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kServerInfoUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token),
      account_(account)
{
}

void ServerInfoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }
    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    ServerInfo ret;

    if (dict.contains("version")) {
        ret.parseVersionFromString(dict["version"].toString());
    }

    if (dict.contains("features")) {
        ret.parseFeatureFromStrings(dict["features"].toStringList());
    }

    if (dict.contains("desktop-custom-logo")) {
        ret.customLogo = dict["desktop-custom-logo"].toString();
    }

    if (dict.contains("desktop-custom-brand")) {
        ret.customBrand = dict["desktop-custom-brand"].toString();
    }

    emit success(account_, ret);
}

LogoutDeviceRequest::LogoutDeviceRequest(const Account& account, bool remove_cache)
    : SeafileApiRequest(account.getAbsoluteUrl(kLogoutDeviceUrl),
                        SeafileApiRequest::METHOD_POST,
                        account.token),
      account_(account),
      remove_cache_(remove_cache)
{
}

void LogoutDeviceRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

GetRepoTokensRequest::GetRepoTokensRequest(const Account& account,
                                           const QStringList& repo_ids)
    : SeafileApiRequest(account.getAbsoluteUrl(kGetRepoTokensUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
    setUrlParam("repos", repo_ids.join(","));
}

void GetRepoTokensRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("GetRepoTokensRequest: failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);
    foreach (const QString& repo_id, dict.keys()) {
        repo_tokens_[repo_id] = dict[repo_id].toString();
    }

    emit success();
}

GetLoginTokenRequest::GetLoginTokenRequest(const Account& account,
                                           const QString& next_url)
    : SeafileApiRequest(account.getAbsoluteUrl(kGetLoginTokenUrl),
                        SeafileApiRequest::METHOD_POST,
                        account.token),
      account_(account),
      next_url_(next_url)
{
}

void GetLoginTokenRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("GetLoginTokenRequest: failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);
    if (!dict.contains("token")) {
        emit failed(ApiError::fromJsonError());
        return;
    }
    emit success(dict["token"].toString());
}

FileSearchRequest::FileSearchRequest(const Account& account,
                                     const QString& keyword,
                                     int per_page)
    : SeafileApiRequest(account.getAbsoluteUrl(kFileSearchUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token),
      keyword_(keyword)
{
    setUrlParam("q", keyword_);
}

void FileSearchRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("FileSearchResult: failed to parse jsn:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }
    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);
    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);
    if (!dict.contains("results")) {
        emit failed(ApiError::fromJsonError());
        return;
    }
    QList<QVariant> results = dict["results"].toList();
    std::vector<FileSearchResult> retval;
    Q_FOREACH(const QVariant& result, results)
    {
        FileSearchResult tmp;
        QMap<QString, QVariant> map = result.toMap();
        if (map.empty())
            continue;
        tmp.repo_id = map["repo_id"].toString();
        // tmp.repo_name = RepoService::instance()->getRepo(tmp.repo_id).name;
        tmp.name = map["name"].toString();
        tmp.oid = map["oid"].toString();
        tmp.last_modified = map["last_modified"].toLongLong();
        tmp.fullpath = map["fullpath"].toString();
        tmp.size = map["size"].toLongLong();
        retval.push_back(tmp);
    }
    emit success(retval);
}

FetchCustomLogoRequest::FetchCustomLogoRequest(const QUrl& url)
    : SeafileApiRequest(url, SeafileApiRequest::METHOD_GET)
{
    setUseCache(true);
}

void FetchCustomLogoRequest::requestSuccess(QNetworkReply& reply)
{
    QPixmap logo;
    logo.loadFromData(reply.readAll());

    if (logo.isNull()) {
        qWarning("FetchCustomLogoRequest: invalid image data\n");
        emit failed(ApiError::fromHttpError(400));
    }
    else {
        emit success(url());
    }
}

FetchAccountInfoRequest::FetchAccountInfoRequest(const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kAccountInfoUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
    account_ = account;
}

void FetchAccountInfoRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("FetchAccountInfoRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QMap<QString, QVariant> dict = mapFromJSON(json.data(), &error);

    AccountInfo info;
    info.email = dict["email"].toString();
    info.name = dict["name"].toString();
    info.totalStorage = dict["total"].toLongLong();
    info.usedStorage = dict["usage"].toLongLong();
    if (info.name.isEmpty()) {
        info.name = dict["nickname"].toString();
    }
    emit success(info);
}

PrivateShareRequest::PrivateShareRequest(const Account& account,
                                         const QString& repo_id,
                                         const QString& path,
                                         const QString& username,
                                         int group_id,
                                         SharePermission permission,
                                         ShareType share_type,
                                         ShareOperation op)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kDirSharedItemsUrl).arg(repo_id)),
          op == UPDATE_SHARE ? METHOD_POST : (op == REMOVE_SHARE ? METHOD_DELETE
                                                                 : METHOD_PUT),
          account.token),
      group_id_(share_type == SHARE_TO_GROUP ? group_id : -1),
      username_(share_type == SHARE_TO_USER ? username : QString()),
      permission_(permission),
      share_type_(share_type),
      share_operation_(op)
{
    setUrlParam("p", path);
    setFormParam("permission", permission == READ_ONLY ? "r" : "rw");
    bool is_add = op == ADD_SHARE;
    if (is_add) {
        setFormParam("share_type",
                     share_type == SHARE_TO_USER ? "user" : "group");
    }
    else {
        setUrlParam("share_type",
                    share_type == SHARE_TO_USER ? "user" : "group");
    }

    if (share_type == SHARE_TO_USER) {
        if (is_add) {
            setFormParam("username", username);
        }
        else {
            setUrlParam("username", username);
        }
    }
    else {
        if (is_add) {
            setFormParam("group_id", QString::number(group_id));
        }
        else {
            setUrlParam("group_id", QString::number(group_id));
        }
    }
}

void PrivateShareRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("PrivateShareRequest: failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    emit success();
}


FetchGroupsAndContactsRequest::FetchGroupsAndContactsRequest(
    const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kFetchGroupsAndContactsUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
}

void FetchGroupsAndContactsRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("FetchGroupsAndContactsRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QList<SeafileGroup> groups;
    QList<SeafileUser> contacts;

    json_t* groups_array = json_object_get(json.data(), "groups");
    if (groups_array) {
        int i, n = json_array_size(groups_array);
        for (i = 0; i < n; i++) {
            json_t* group_object = json_array_get(groups_array, i);
            const char* name =
                json_string_value(json_object_get(group_object, "name"));
            int group_id =
                json_integer_value(json_object_get(group_object, "id"));
            if (name && group_id) {
                SeafileGroup group;
                group.id = group_id;
                group.name = QString::fromUtf8(name);
                const char* owner =
                    json_string_value(json_object_get(group_object, "creator"));
                if (owner) {
                    group.owner = QString::fromUtf8(owner);
                }
                groups.push_back(group);
            }
        }
    }

    json_t* contacts_array = json_object_get(json.data(), "contacts");
    if (contacts_array) {
        int i, n = json_array_size(contacts_array);

        for (i = 0; i < n; i++) {
            json_t* contact_object = json_array_get(contacts_array, i);
            const char* email =
                json_string_value(json_object_get(contact_object, "email"));
            if (email) {
                SeafileUser contact;
                contact.email = QString::fromUtf8(email);
                contact.name = QString::fromUtf8(
                    json_string_value(json_object_get(contact_object, "name")));
                contacts.push_back(contact);
            }
        }
    }

    emit success(groups, contacts);
}

GetPrivateShareItemsRequest::GetPrivateShareItemsRequest(const Account& account,
                                                         const QString& repo_id,
                                                         const QString& path)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kDirSharedItemsUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_GET,
          account.token)
{
    setUrlParam("p", path);
}

void GetPrivateShareItemsRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("GetPrivateShareItemsRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QList<GroupShareInfo> group_shares;
    QList<UserShareInfo> user_shares;

    int i, n = json_array_size(json.data());
    for (i = 0; i < n; i++) {
        json_t* share_info_object = json_array_get(json.data(), i);
        Json share_info(share_info_object);
        QString share_type = share_info.getString("share_type");
        QString permission = share_info.getString("permission");
        if (share_type == "group") {
            // group share
            Json group = share_info.getObject("group_info");
            GroupShareInfo group_share;
            group_share.group.id = group.getLong("id");
            group_share.group.name = group.getString("name");
            group_share.permission = ::permissionfromString(permission);
            group_shares.push_back(group_share);
        }
        else if (share_type == "user") {
            Json user = share_info.getObject("user_info");
            UserShareInfo user_share;
            user_share.user.email = user.getString("name");
            user_share.user.name = user.getString("nickname");
            user_share.permission = ::permissionfromString(permission);
            user_shares.push_back(user_share);
        }
    }

    emit success(group_shares, user_shares);
}

RemoteWipeReportRequest::RemoteWipeReportRequest(const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kRemoteWipeReportUrl),
                        SeafileApiRequest::METHOD_POST)
{
    setFormParam(QString("token"), account.token);
}

void RemoteWipeReportRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

SearchUsersRequest::SearchUsersRequest(
    const Account& account, const QString& pattern)
    : SeafileApiRequest(account.getAbsoluteUrl(kSearchUsersUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token),
      pattern_(pattern)
{
    setUrlParam("q", pattern);
}

void SearchUsersRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("SearchUsersRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QList<SeafileUser> users;

    json_t* users_array = json_object_get(json.data(), "users");
    if (users_array) {
        int i, n = json_array_size(users_array);

        for (i = 0; i < n; i++) {
            json_t* user_object = json_array_get(users_array, i);
            const char* email =
                json_string_value(json_object_get(user_object, "email"));
            if (email) {
                SeafileUser user;
                user.email = QString::fromUtf8(email);
                user.name = QString::fromUtf8(
                    json_string_value(json_object_get(user_object, "name")));
                user.contact_email = QString::fromUtf8(
                    json_string_value(json_object_get(user_object, "user_email")));
                user.avatar_url = QString::fromUtf8(
                    json_string_value(json_object_get(user_object, "avatar_url")));
                users.push_back(user);
            }
        }
    }

    emit success(users);
}


FetchGroupsRequest::FetchGroupsRequest(
    const Account& account)
    : SeafileApiRequest(account.getAbsoluteUrl(kFetchGroupsUrl),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
    setUrlParam("with_msg", "false");
}

void FetchGroupsRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    json_t* root = parseJSON(reply, &error);
    if (!root) {
        qWarning("FetchGroupsRequest: failed to parse json:%s\n",
                 error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QList<SeafileGroup> groups;

    int i, n = json_array_size(json.data());
    for (i = 0; i < n; i++) {
        json_t* group_object = json_array_get(json.data(), i);
        const char* name =
            json_string_value(json_object_get(group_object, "name"));
        int group_id =
            json_integer_value(json_object_get(group_object, "id"));
        if (name && group_id) {
            SeafileGroup group;
            group.id = group_id;
            group.name = QString::fromUtf8(name);
            const char* owner =
                json_string_value(json_object_get(group_object, "creator"));
            if (owner) {
                group.owner = QString::fromUtf8(owner);
            }
            groups.push_back(group);
        }
    }

    emit success(groups);
}

GetDirentsRequest::GetDirentsRequest(const Account& account,
                                     const QString& repo_id,
                                     const QString& path)
    : SeafileApiRequest (account.getAbsoluteUrl(QString(kGetDirentsUrl).arg(repo_id)),
                         SeafileApiRequest::METHOD_GET, account.token),
      repo_id_(repo_id), path_(path), readonly_(false)
{
    setUrlParam("p", path);
}

void GetDirentsRequest::requestSuccess(QNetworkReply& reply)
{
    json_error_t error;
    QString dir_id = reply.rawHeader("oid");
    if (dir_id.length() != 40) {
        emit failed(ApiError::fromHttpError(500));
        return;
    }
    // this extra header column only supported from v4.2 seahub
    readonly_ = reply.rawHeader("dir_perm") == "r";

    json_t *root = parseJSON(reply, &error);
    if (!root) {
        qDebug("GetDirentsRequest: failed to parse json:%s\n", error.text);
        emit failed(ApiError::fromJsonError());
        return;
    }

    QScopedPointer<json_t, JsonPointerCustomDeleter> json(root);

    QList<SeafDirent> dirents;
    dirents = SeafDirent::listFromJSON(json.data(), &error);
    emit success(readonly_, dirents);
}

GetFileDownloadLinkRequest::GetFileDownloadLinkRequest(const Account &account,
                                                       const QString &repo_id,
                                                       const QString &path)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kGetFilesUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_GET, account.token)
{
    setUrlParam("p", path);
}

void GetFileDownloadLinkRequest::requestSuccess(QNetworkReply& reply)
{
    QString reply_content(reply.readAll());
    QString oid;

    if (reply.hasRawHeader("oid"))
        oid = reply.rawHeader("oid");

    do {
        if (reply_content.size() <= 2)
            break;
        reply_content.remove(0, 1);
        reply_content.chop(1);
        QUrl new_url(reply_content);

        if (!new_url.isValid())
            break;

        file_id_ = oid;
        emit success(reply_content);
        return;
    } while (0);
    emit failed(ApiError::fromHttpError(500));
}

GetSharedLinkRequest::GetSharedLinkRequest(const Account &account,
                                           const QString &repo_id,
                                           const QString &path,
                                           bool is_file)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kGetFileSharedLinkUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_PUT, account.token)
{
    setFormParam("type", is_file ? "f" : "d");
    setFormParam("p", path);
}

void GetSharedLinkRequest::requestSuccess(QNetworkReply& reply)
{
    QString reply_content(reply.rawHeader("Location"));

    emit success(reply_content);
}

CreateDirectoryRequest::CreateDirectoryRequest(const Account &account,
                                               const QString &repo_id,
                                               const QString &path,
                                               bool create_parents)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kGetDirentsUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_POST, account.token),
      repo_id_(repo_id), path_(path), create_parents_(create_parents)
{
    setUrlParam("p", path);

    setFormParam("operation", "mkdir");
    setFormParam("create_parents", create_parents ? "true" : "false");
}

void CreateDirectoryRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

GetFileUploadLinkRequest::GetFileUploadLinkRequest(const Account &account,
                                                   const QString &repo_id,
                                                   const QString &path,
                                                   bool use_upload)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(
              use_upload ? kGetFileUploadUrl : kGetFileUpdateUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_GET, account.token)
{
    setUrlParam("p", path);
}

void GetFileUploadLinkRequest::requestSuccess(QNetworkReply& reply)
{
    QString reply_content(reply.readAll());

    do {
        if (reply_content.size() <= 2)
            break;
        reply_content.remove(0, 1);
        reply_content.chop(1);
        QUrl new_url(reply_content);

        if (!new_url.isValid())
            break;

        emit success(reply_content);
        return;
    } while (0);
    emit failed(ApiError::fromHttpError(500));
}

RenameDirentRequest::RenameDirentRequest(const Account &account,
                                         const QString &repo_id,
                                         const QString &path,
                                         const QString &new_name,
                                         bool is_file)
    : SeafileApiRequest(
        account.getAbsoluteUrl(
            QString(is_file ? kGetFilesUrl: kGetDirentsUrl).arg(repo_id)),
        SeafileApiRequest::METHOD_POST, account.token),
    is_file_(is_file), repo_id_(repo_id), path_(path), new_name_(new_name)
{
    setUrlParam("p", path);

    setFormParam("operation", "rename");
    setFormParam("newname", new_name);
}

void RenameDirentRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

RemoveDirentRequest::RemoveDirentRequest(const Account &account,
                                         const QString &repo_id,
                                         const QString &path,
                                         bool is_file)
    : SeafileApiRequest(
        account.getAbsoluteUrl(
            QString(is_file ? kGetFilesUrl : kGetDirentsUrl).arg(repo_id)),
        SeafileApiRequest::METHOD_DELETE, account.token),
    is_file_(is_file), repo_id_(repo_id), path_(path)
{
    setUrlParam("p", path);
}

void RemoveDirentRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

MoveFileRequest::MoveFileRequest(const Account &account,
                                 const QString &repo_id,
                                 const QString &path,
                                 const QString &dst_repo_id,
                                 const QString &dst_dir_path)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kGetFilesUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_POST, account.token)
{
    setUrlParam("p", path);

    setFormParam("operation", "move");
    setFormParam("dst_repo", dst_repo_id);
    setFormParam("dst_dir", dst_dir_path);
}

void MoveFileRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

CopyMultipleFilesRequest::CopyMultipleFilesRequest(const Account &account,
                                                   const QString &repo_id,
                                                   const QString &src_dir_path,
                                                   const QStringList &src_file_names,
                                                   const QString &dst_repo_id,
                                                   const QString &dst_dir_path)
    : SeafileApiRequest(
        account.getAbsoluteUrl(QString(kFileOperationCopy).arg(repo_id)),
    SeafileApiRequest::METHOD_POST, account.token),
    repo_id_(repo_id),
    src_dir_path_(src_dir_path),
    src_file_names_(src_file_names)
{
    setUrlParam("p", src_dir_path);

    setFormParam("file_names", src_file_names.join(":"));
    setFormParam("dst_repo", dst_repo_id);
    setFormParam("dst_dir", dst_dir_path);
}

void CopyMultipleFilesRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

MoveMultipleFilesRequest::MoveMultipleFilesRequest(const Account &account,
                                                   const QString &repo_id,
                                                   const QString &src_dir_path,
                                                   const QStringList &src_file_names,
                                                   const QString &dst_repo_id,
                                                   const QString &dst_dir_path)
    : SeafileApiRequest(
        account.getAbsoluteUrl(QString(kFileOperationMove).arg(repo_id)),
    SeafileApiRequest::METHOD_POST, account.token),
    repo_id_(repo_id),
    src_dir_path_(src_dir_path),
    src_file_names_(src_file_names)
{
    setUrlParam("p", src_dir_path);

    setFormParam("file_names", src_file_names.join(":"));
    setFormParam("dst_repo", dst_repo_id);
    setFormParam("dst_dir", dst_dir_path);
}

void MoveMultipleFilesRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

StarFileRequest::StarFileRequest(const Account &account,
                                 const QString &repo_id,
                                 const QString &path)
    : SeafileApiRequest(
          account.getAbsoluteUrl(kGetStarredFilesUrl),
          SeafileApiRequest::METHOD_POST, account.token)
{
    setFormParam("repo_id", repo_id);
    setFormParam("p", path);
}

void StarFileRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

UnstarFileRequest::UnstarFileRequest(const Account &account,
                                     const QString &repo_id,
                                     const QString &path)
    : SeafileApiRequest(
          account.getAbsoluteUrl(kGetStarredFilesUrl),
          SeafileApiRequest::METHOD_DELETE, account.token)
{
    setUrlParam("repo_id", repo_id);
    setUrlParam("p", path);
}

void UnstarFileRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

LockFileRequest::LockFileRequest(const Account &account, const QString &repo_id,
                                 const QString &path, bool lock)
    : SeafileApiRequest(
          account.getAbsoluteUrl(QString(kGetFilesUrl).arg(repo_id)),
          SeafileApiRequest::METHOD_PUT, account.token),
      lock_(lock), repo_id_(repo_id), path_(path)
{
    setFormParam("p", path.startsWith("/") ? path : "/" + path);

    setFormParam("operation", lock ? "lock" : "unlock");
}

void LockFileRequest::requestSuccess(QNetworkReply& reply)
{
    emit success();
}

/**
 * AuthPingRequest
 */
AuthPingRequest::AuthPingRequest(const Account &account)
    : SeafileApiRequest(account.getAbsoluteUrl(kAuthPingURL),
                        SeafileApiRequest::METHOD_GET,
                        account.token)
{
}

void AuthPingRequest::requestSuccess(QNetworkReply &reply)
{
    emit success();
}
