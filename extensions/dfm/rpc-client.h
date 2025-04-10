#ifndef SEADRIVERPCCLIENT_H
#define SEADRIVERPCCLIENT_H

#include <string>

#include <searpc-client.h>
#include <searpc-named-pipe-transport.h>

enum LockState {
      FILE_NOT_LOCKED = 0,
      FILE_LOCKED_BY_OTHERS,
      FILE_LOCKED_BY_ME_MANUAL,
      FILE_LOCKED_BY_ME_AUTO
};

namespace SeaDrivePlugin {

class SeaDriveRpcClient {
public:
    SeaDriveRpcClient();
    ~SeaDriveRpcClient();
    void connectDaemon();
    bool tryConnectDaemon();

    std::string getMountDir() const { return mount_dir_; }
    bool isConnected() const { return connected_; }

    int lockFile (const char *path);
    int unlockFile (const char *path);
    int getFileLockState (const char *path);
    int getShareLink (const char *path);
    int getInternalLink (const char *path);
    int getUploadLink (const char *path);
    int showFileHistory (const char *path);
    char *getFileCacheState (const char *path);
    int isPathInRepo(const char *path);

private:
    SearpcClient *seadrive_rpc_client_;

    std::string mount_dir_;
    std::string seadrive_dir_;
    bool connected_;
};

}

#endif // SEADRIVERPCCLIENT_H
