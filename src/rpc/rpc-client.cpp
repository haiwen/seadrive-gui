extern "C" {

#include <searpc-client.h>
#include <searpc-named-pipe-transport.h>

}

#include <QtDebug>
#include <QMutexLocker>

#include "seadrive-gui.h"
#include "settings-mgr.h"

#include "account.h"
#include "utils/utils.h"
#include "api/commit-details.h"
#include "message-poller.h"
#include "rpc-client.h"
#include "utils/utils-win.h"
#include "daemon-mgr.h"


namespace {

#if defined(Q_OS_WIN32)
const char *kSeadriveSockName = "\\\\.\\pipe\\seadrive_";
#else
const char *kSeadriveSockName = "seadrive.sock";
#endif

const char *kSeadriveRpcService = "seadrive-rpcserver";

} // namespace

SeafileRpcClient::SeafileRpcClient()
    : seadrive_rpc_client_(0),
      connected_(false)
{
}

SeafileRpcClient::~SeafileRpcClient()
{
    if (seadrive_rpc_client_) {
        searpc_free_client_with_pipe_transport(seadrive_rpc_client_);
        seadrive_rpc_client_ = 0;
    }
    connected_ = false;
}

void SeafileRpcClient::connectDaemon()
{
    int retry = 0;
    SearpcNamedPipeClient *pipe_client;
    while (true) {
#if defined(Q_OS_WIN32)
        pipe_client = searpc_create_named_pipe_client(
            utils::win::getLocalPipeName(kSeadriveSockName).c_str());
#else
        pipe_client = searpc_create_named_pipe_client(
            toCStr(QDir(gui->daemonManager()->currentCacheDir()).filePath(kSeadriveSockName)));
#endif
        if (searpc_named_pipe_client_connect(pipe_client) < 0) {
            if (retry++ > 5) {
                gui->errorAndExit(tr("internal error: failed to connect to seadrive daemon"));
                return;
            } else {
                g_usleep(500000);
            }
        } else {
            break;
        }
    }
    connected_ = true;
    seadrive_rpc_client_ = searpc_client_with_named_pipe_transport(
        pipe_client, kSeadriveRpcService);
}

int SeafileRpcClient::seafileGetConfig(const QString &key, QString *value)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (seadrive_rpc_client_,
                                            "seafile_get_config", &error,
                                            1, "string", toCStr(key));
    if (error) {
        qWarning("Unable to get config value %s: %s", key.toUtf8().data(), error->message);
        g_error_free(error);
        return -1;
    }
    *value = QString::fromUtf8(ret);

    g_free (ret);
    return 0;
}

int SeafileRpcClient::seafileGetConfigInt(const QString &key, int *value)
{
    GError *error = NULL;
    *value = searpc_client_call__int (seadrive_rpc_client_,
                                      "seafile_get_config_int", &error,
                                      1, "string", toCStr(key));
    if (error) {
        qWarning("Unable to get config value %s: %s", key.toUtf8().data(), error->message);
        g_error_free(error);
        return -1;
    }
    return 0;
}


int SeafileRpcClient::seafileSetConfig(const QString &key, const QString &value)
{
    // printf ("set config: %s = %s\n", toCStr(key), toCStr(value));
    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_set_config", &error,
                             2, "string", toCStr(key),
                             "string", toCStr(value));
    if (error) {
        qWarning("Unable to set config value %s", key.toUtf8().data());
        g_error_free(error);
        return -1;
    }
    return 0;
}

int SeafileRpcClient::setUploadRateLimit(int limit)
{
    return setRateLimit(true, limit);
}

int SeafileRpcClient::setDownloadRateLimit(int limit)
{
    return setRateLimit(false, limit);
}

int SeafileRpcClient::setRateLimit(bool upload, int limit)
{
    GError *error = NULL;
    const char *rpc = upload ? "seafile_set_upload_rate_limit" : "seafile_set_download_rate_limit";
    searpc_client_call__int (seadrive_rpc_client_,
                             rpc, &error,
                             1, "int", limit);
    if (error) {
        g_error_free(error);
        return -1;
    }
    return 0;
}

int SeafileRpcClient::seafileSetConfigInt(const QString &key, int value)
{
    // printf ("set config: %s = %d\n", toCStr(key), value);
    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_set_config_int", &error,
                             2, "string", toCStr(key),
                             "int", value);
    if (error) {
        g_error_free(error);
        return -1;
    }
    return 0;
}

int SeafileRpcClient::getDownloadRate(int *rate)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (seadrive_rpc_client_,
                                       "seafile_get_download_rate",
                                       &error, 0);

    if (error) {
        g_error_free(error);
        return -1;
    }

    *rate = ret;
    return 0;
}

int SeafileRpcClient::getUploadRate(int *rate)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (seadrive_rpc_client_,
                                       "seafile_get_upload_rate",
                                       &error, 0);

    if (error) {
        g_error_free(error);
        return -1;
    }

    *rate = ret;
    return 0;
}

bool SeafileRpcClient::getUploadProgress(json_t **ret_obj)
{
    GError *error = NULL;
    json_t *ret = searpc_client_call__json (
        seadrive_rpc_client_,
        "seafile_get_upload_progress",
        &error, 0);
    if (error) {
        qWarning("failed to get upload progress: %s\n",
                 error->message ? error->message : "");
        g_error_free(error);
        return false;
    }

    *ret_obj = ret;

    return true;
}

bool SeafileRpcClient::getDownloadProgress(json_t **ret_obj)
{
    GError *error = NULL;
    json_t *ret = searpc_client_call__json (
        seadrive_rpc_client_,
        "seafile_get_download_progress",
        &error, 0);
    if (error) {
        qWarning("failed to get download progress: %s\n",
                 error->message ? error->message : "");
        g_error_free(error);
        return false;
    }

    *ret_obj = ret;

    return true;
}

int SeafileRpcClient::getRepoProperty(const QString &repo_id,
                                      const QString& name,
                                      QString *value)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (
        seadrive_rpc_client_,
        "seafile_get_repo_property",
        &error, 2,
        "string", toCStr(repo_id),
        "string", toCStr(name)
        );
    if (error) {
        g_error_free(error);
        return -1;
    }
    *value = QString::fromUtf8(ret);

    g_free(ret);
    return 0;
}

int SeafileRpcClient::setRepoProperty(const QString &repo_id,
                                      const QString& name,
                                      const QString& value)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (
        seadrive_rpc_client_,
        "seafile_set_repo_property",
        &error, 3,
        "string", toCStr(repo_id),
        "string", toCStr(name),
        "string", toCStr(value)
        );
    if (error) {
        g_error_free(error);
        return -1;
    }
    return ret;
}


int SeafileRpcClient::setRepoToken(const QString &repo_id,
                                   const QString& token)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (
        seadrive_rpc_client_,
        "seafile_set_repo_token",
        &error, 2,
        "string", toCStr(repo_id),
        "string", toCStr(token)
        );
    if (error) {
        g_error_free(error);
        return -1;
    }
    return ret;
}

int SeafileRpcClient::getRepoFileStatus(const QString& repo_uname,
                                        const QString& path_in_repo,
                                        QString *status)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (
        seadrive_rpc_client_,
        "seafile_get_path_sync_status",
        &error, 2,
        "string", toCStr(repo_uname),
        "string", toCStr(path_in_repo));
    if (error) {
        qWarning("failed to get path status for %s/%s: %s\n",
                 toCStr(repo_uname),
                 toCStr(path_in_repo),
                 error->message);
        g_error_free(error);
        return -1;
    }

    *status = ret;

    // printf ("status for %s/%s = %s\n", toCStr(repo_uname), toCStr(path_in_repo), ret);

    g_free (ret);
    return 0;
}

int SeafileRpcClient::getCategorySyncStatus(const QString& category, QString *status)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (
        seadrive_rpc_client_,
        "seafile_get_category_sync_status",
        &error, 1,
        "string", toCStr(category));
    if (error) {
        qWarning("failed to get category status for %s: %s\n",
                 toCStr(category),
                 error->message);
        g_error_free(error);
        return -1;
    }

    *status = ret;

    // printf ("status for %s = %s\n", toCStr(category), ret);

    g_free (ret);
    return 0;
}

int SeafileRpcClient::markFileLockState(const QString &repo_id,
                                        const QString &path_in_repo,
                                        bool lock)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (
        seadrive_rpc_client_,
        lock ? "seafile_mark_file_locked" : "seafile_mark_file_unlocked",
        &error, 2,
        "string", toCStr(repo_id),
        "string", toCStr(path_in_repo));
    if (error) {
        qWarning("failed to mark file lock state: %s\n", error->message);
        g_error_free(error);
        return -1;
    }

    g_free (ret);
    return 0;
}


bool SeafileRpcClient::setServerProperty(const QString &url,
                                         const QString &key,
                                         const QString &value)
{
    // printf("set server config: %s %s = %s\n", toCStr(url), toCStr(key),
    //        toCStr(value));
    GError *error = NULL;
    searpc_client_call__int(seadrive_rpc_client_, "seafile_set_server_property",
                            &error, 3, "string", toCStr(url), "string",
                            toCStr(key), "string", toCStr(value));
    if (error) {
        qWarning("Unable to set server property %s %s", toCStr(url),
                 toCStr(key));
        g_error_free(error);
        return false;
    }
    return true;
}


bool SeafileRpcClient::switchAccount(const Account& account)
{
    GError *error = NULL;
    QString serverAddr = account.serverUrl.toString();
    if (serverAddr.endsWith("/")) {
        serverAddr = serverAddr.left(serverAddr.size() - 1);
    }
    searpc_client_call__int(seadrive_rpc_client_,
                            "seafile_switch_account",
                            &error,
                            4,
                            "string",
                            toCStr(serverAddr),
                            "string",
                            toCStr(account.username),
                            "string",
                            toCStr(account.token),
                            "int",
                            account.isPro() ? 1 : 0);
    if (error) {
        qWarning() << "Unable to switch to account" << account << ":"
                   << (error->message ? error->message : "");
        g_error_free(error);
        return false;
    }
    qWarning() << "Switched to account" << account;
    return true;
}

bool SeafileRpcClient::switchAccount(const Account& account, bool ispro)
{
    GError *error = NULL;
    QString serverAddr = account.serverUrl.toString();
    if (serverAddr.endsWith("/")) {
        serverAddr = serverAddr.left(serverAddr.size() - 1);
    }
    searpc_client_call__int(seadrive_rpc_client_,
                            "seafile_switch_account",
                            &error,
                            4,
                            "string",
                            toCStr(serverAddr),
                            "string",
                            toCStr(account.username),
                            "string",
                            toCStr(account.token),
                            "int",
                            ispro ? 1 : 0);
    if (error) {
        qWarning() << "Unable to switch to account" << account << ":"
                   << (error->message ? error->message : "");
        g_error_free(error);
        return false;
    }
    qWarning() << "Switched to account" << account;
    return true;
}

bool SeafileRpcClient::deleteAccount(const Account& account, bool remove_cache)
{
    GError *error = NULL;
    searpc_client_call__int(seadrive_rpc_client_,
                            "seafile_delete_account",
                            &error,
                            3,
                            "string",
                            toCStr(account.serverUrl.toString()),
                            "string",
                            toCStr(account.username),
                            "int",
                            remove_cache ? 1 : 0);
    if (error) {
        qWarning() << "Unable to delete account" << account << ":"
                   << (error->message ? error->message : "");
        g_error_free(error);
        return false;
    }
    qWarning() << "Deleted account" << account;
    return true;
}

bool SeafileRpcClient::getRepoIdByPath(const QString &repo_uname,
                                       QString *repo_id)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (
        seadrive_rpc_client_,
        "seafile_get_repo_id_by_uname",
        &error, 1,
        "string", toCStr(repo_uname));
    if (error) {
        qWarning("failed to get repo id of %s: %s\n",
                 toCStr(repo_uname),
                 error->message);
        g_error_free(error);
        return false;
    }

    *repo_id = ret;

    g_free(ret);
    return true;
}

bool SeafileRpcClient::getRepoUnameById(const QString &repo_id,
                                        QString *repo_uname)
{
    GError *error = NULL;
    char *ret = searpc_client_call__string (
        seadrive_rpc_client_,
        "seafile_get_repo_display_name_by_id",
        &error, 1,
        "string", toCStr(repo_id));
    if (error) {
        qWarning("failed to get repo uname of %s: %s\n",
                 toCStr(repo_id),
                 error->message);
        g_error_free(error);
        return false;
    }

    if (!ret) {
        return false;
    }

    *repo_uname = ret;

    g_free(ret);
    return true;
}


bool SeafileRpcClient::getSyncNotification(json_t **ret_obj)
{
    GError *error = NULL;
    json_t *ret = searpc_client_call__json (
        seadrive_rpc_client_,
        "seafile_get_sync_notification",
        &error, 0);
    if (error) {
        qWarning("failed to get sync notification: %s\n",
                 error->message ? error->message : "");
        g_error_free(error);
        return false;
    }

    if (!ret) {
        // No pending notifications.
        return false;
    }

    *ret_obj = ret;

    return true;
}

bool SeafileRpcClient::getGlobalSyncStatus(json_t **ret_obj)
{
    GError *error = NULL;
    json_t *ret = searpc_client_call__json (
        seadrive_rpc_client_,
        "seafile_get_global_sync_status",
        &error, 0);
    if (error || !ret) {
        qWarning("failed to get global sync status: %s\n",
                 (error && error->message) ? error->message : "");
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    *ret_obj = ret;

    return true;
}

bool SeafileRpcClient::unmount()
{
    GError *error = NULL;
    int ret = searpc_client_call__int (
        seadrive_rpc_client_,
        "seafile_unmount",
        &error, 0);
    if (error || ret != 0) {
        // The unmount rpc would case seadrive daemon to exit, so the rpc would always fail.
        qDebug("failed to unmount : %s\n",
               (error && error->message) ? error->message : "");
        if (error) {
            g_error_free(error);
        }
        return false;
    }

    return true;
}

bool SeafileRpcClient::setCacheCleanIntervalMinutes(int interval)
{
    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_set_clean_cache_interval",
                             &error,
                             1, "int", interval * 60);
    if (error) {
        g_error_free(error);
        return false;
    }
    return true;
}

bool SeafileRpcClient::setCacheSizeLimitGB(int limit)
{
    GError *error = NULL;
    gint64 limit_in_bytes = ((gint64)limit) * 1e9;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_set_cache_size_limit",
                             &error,
                             1, "int64", &limit_in_bytes);
    if (error) {
        g_error_free(error);
        return false;
    }
    return true;
}

bool SeafileRpcClient::getCacheCleanIntervalMinutes(int *value)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (seadrive_rpc_client_,
                                       "seafile_get_clean_cache_interval",
                                       &error, 0);

    if (error) {
        g_error_free(error);
        return false;
    }

    *value = ret / 60;
    return true;
}

bool SeafileRpcClient::getCacheSizeLimitGB(int *value)
{
    GError *error = NULL;
    gint64 ret = searpc_client_call__int64 (seadrive_rpc_client_,
                                            "seafile_get_cache_size_limit",
                                            &error, 0);

    if (error) {
        g_error_free(error);
        return false;
    }

    *value = (int)(ret / 1e9);
    return true;
}

bool SeafileRpcClient::getSeaDriveEvents(json_t **ret_obj)
{
    GError *error = NULL;
    json_t *ret = searpc_client_call__json (
        seadrive_rpc_client_,
        "seafile_get_events_notification",
        &error, 0);
    if (error) {
        qWarning("failed to get seadrive.events: %s\n",
                 error->message ? error->message : "");
        g_error_free(error);
        return false;
    }

    if (!ret) {
        return false;
    }

    *ret_obj = ret;
    return true;
}

bool SeafileRpcClient::getSyncErrors(json_t **ret_obj)
{
    GError *error = NULL;
    json_t *ret = searpc_client_call__json (
        seadrive_rpc_client_,
        "seafile_list_sync_errors",
        &error, 0);
    if (error) {
        qWarning("failed to get sync errors: %s\n",
                 error->message ? error->message : "");
        g_error_free(error);
        return false;
    }

    if (!ret) {
        // No pending errors.
        return false;
    }

    *ret_obj = ret;

    return true;
}

bool SeafileRpcClient::cachePath(const QString& repo_id,
                                 const QString& path_in_repo)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (
        seadrive_rpc_client_,
        "seafile_cache_path",
        &error, 2, "string", toCStr(repo_id),
        "string", toCStr(path_in_repo));

    if (error || ret != 0) {
        qWarning("failed to cache %s/%s, errors: %s.\n",
                 toCStr(repo_id), toCStr(path_in_repo),
                 error->message ? error->message : "");
        if (error) {
            g_error_free(error);
        }
        return false;
    } else {
        return true;
    }
}


bool SeafileRpcClient::isFileCached(const QString& repo_id,
                                    const QString& path_in_repo)
{
    GError *error = NULL;
    int ret = searpc_client_call__int (
        seadrive_rpc_client_,
        "seafile_is_file_cached",
        &error, 2, "string", toCStr(repo_id),
        "string", toCStr(path_in_repo));

    if (error) {
        qWarning("failed to check file cached %s/%s, errors: %s.\n",
                 toCStr(repo_id), toCStr(path_in_repo),
                 error->message ? error->message : "");
        g_error_free(error);
        return false;
    } else {
        // This rpc returns 1 if file is cached and 0 otherwise
        return ret != 0;
    }
}
