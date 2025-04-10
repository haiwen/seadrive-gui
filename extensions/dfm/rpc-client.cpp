#include "rpc-client.h"
#include <unistd.h>
#include <pwd.h>
#include "log.h"
#include <errno.h>

namespace SeaDrivePlugin {

// USING_DFMEXT_NAMESPACE

const char *kSeadriveSockName = "seadrive.sock";
const char *kSeadriveRpcService = "seadrive-rpcserver";

SeaDriveRpcClient::SeaDriveRpcClient()
    : seadrive_rpc_client_(0),
      connected_(false)
{
    struct passwd *pw = getpwuid(getuid());
    std::string homePath {pw->pw_dir}; 
    mount_dir_ = homePath + "/SeaDrive";
    seadrive_dir_ = homePath + "/.seadrive";
}

SeaDriveRpcClient::~SeaDriveRpcClient()
{
    if (seadrive_rpc_client_) {
        searpc_free_client_with_pipe_transport(seadrive_rpc_client_);
        seadrive_rpc_client_ = 0;
    }
      connected_ = false;
}

void SeaDriveRpcClient::connectDaemon()
{
    connected_ = false;
    if (seadrive_rpc_client_) {
        searpc_free_client_with_pipe_transport(seadrive_rpc_client_);
        seadrive_rpc_client_ = 0;
    }

    char *rpc_pipe_path = g_build_filename (seadrive_dir_.c_str(), kSeadriveSockName, NULL);
    SearpcNamedPipeClient *pipe_client;
    pipe_client = searpc_create_named_pipe_client(rpc_pipe_path);
    if (searpc_named_pipe_client_connect(pipe_client) < 0) {
    	seaf_ext_log ("failed to connect name pipe client for path %s: %s\n", rpc_pipe_path, strerror(errno));
        g_free (rpc_pipe_path);
        g_free (pipe_client);
        return;
    }
    g_free (rpc_pipe_path);
    connected_ = true;
    seadrive_rpc_client_ = searpc_client_with_named_pipe_transport(
        pipe_client, kSeadriveRpcService);
}

int SeaDriveRpcClient::lockFile(const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_lock_file", &error,
                             1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return 0;
}

int SeaDriveRpcClient::unlockFile(const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_unlock_file", &error,
                             1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return 0;
}

int SeaDriveRpcClient::getFileLockState (const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    int ret = searpc_client_call__int (seadrive_rpc_client_,
                                       "seafile_get_file_lock_state", &error,
                                       1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return ret;
}

int SeaDriveRpcClient::getShareLink (const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_get_share_link", &error,
                             1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return 0;
}

int SeaDriveRpcClient::getInternalLink (const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_get_internal_link", &error,
                             1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return 0;
}

int SeaDriveRpcClient::getUploadLink (const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_get_upload_link", &error,
                             1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return 0;
}

int SeaDriveRpcClient::showFileHistory (const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    searpc_client_call__int (seadrive_rpc_client_,
                             "seafile_show_file_history", &error,
                             1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return 0;
}

char *SeaDriveRpcClient::getFileCacheState (const char *path)
{
    if (!connected_) {
        return NULL;
    }

    GError *error = NULL;
    char *ret = searpc_client_call__string (seadrive_rpc_client_,
                                            "seafile_get_file_cache_state", &error,
                                            1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return NULL;
    }

    return ret;
}

int SeaDriveRpcClient::isPathInRepo (const char *path)
{
    if (!connected_) {
        return -1;
    }

    GError *error = NULL;
    int ret = searpc_client_call__int (seadrive_rpc_client_,
                                       "seafile_is_path_in_repo", &error,
                                       1, "string", path);
    if (error) {
        g_error_free(error);
        connected_ = false;
        return -1;
    }

    return ret;
}

}
