#include "seadrive-menu-plugin.h"

#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <cassert>
#include <iostream>

#include "log.h"

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

namespace SeaDrivePlugin {
USING_DFMEXT_NAMESPACE

SeaDriveMenuPlugin::SeaDriveMenuPlugin()
    : DFMEXT::DFMExtMenuPlugin()
{
    rpc_client_ = new SeaDrivePlugin::SeaDriveRpcClient;

    registerInitialize([this](DFMEXT::DFMExtMenuProxy *proxy) {
        initialize(proxy);
    });
    registerBuildNormalMenu([this](DFMExtMenu *main, const std::string &currentPath,
                                   const std::string &focusPath, const std::list<std::string> &pathList,
                                   bool onDesktop) {
        return buildNormalMenu(main, currentPath, focusPath, pathList, onDesktop);
    });
    registerBuildEmptyAreaMenu([this](DFMExtMenu *main, const std::string &currentPath, bool onDesktop) {
        return buildEmptyAreaMenu(main, currentPath, onDesktop);
    });
}

SeaDriveMenuPlugin::~SeaDriveMenuPlugin()
{
    if (rpc_client_) {
        delete rpc_client_; 
    }
}

void SeaDriveMenuPlugin::initialize(DFMExtMenuProxy *proxy)
{
    proxy_ = proxy;
}

bool SeaDriveMenuPlugin::buildNormalMenu(DFMExtMenu *main, const std::string &currentPath,
                                   const std::string &focusPath, const std::list<std::string> &pathList,
                                   bool onDesktop)
{
    if (onDesktop) {
        return true;
    }

    if (pathList.size() != 1) {
        return true;
    }

    if (!rpc_client_->isConnected()) {
        rpc_client_->connectDaemon(); 
    }
    if (!rpc_client_->isConnected()) {
        return true;
    }

    // If the file is not in the mount dir, do not set the menu.
    if (pathList.front().rfind(rpc_client_->getMountDir(), 0) != 0) {
        return true;
    }

    // Set first-level menu.
    auto rootAction { proxy_->createAction() };
    rootAction->setText("SeaDrive");

    auto menu { proxy_->createMenu() };

    rootAction->setMenu(menu);
    // Set second-level menu.
    rootAction->registerHovered([this, pathList](DFMExtAction *action) {
        std::string path = pathList.front().substr(rpc_client_->getMountDir().size());
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }
        if (path.empty()) {
            return;
        }
        if (!action->menu()->actions().empty())
            return;

        int inRepo = rpc_client_->isPathInRepo (path.c_str());
        if (inRepo < 0) {
            return;
        }

        struct stat st;
        if (stat(pathList.front().c_str(), &st)!= 0) {
            seaf_ext_log ("Failed to stat path %s: %s.\n", pathList.front().c_str(), strerror(errno));
            return;
        }
        if (S_ISDIR(st.st_mode)) {
            auto uploadLinkAct { proxy_->createAction() };
            uploadLinkAct->setText("获取上传链接");
            uploadLinkAct->registerTriggered([this, path](DFMExtAction *, bool) {
                rpc_client_->getUploadLink (path.c_str());
            });
            action->menu()->addAction(uploadLinkAct);
            return;
        }

        int state = rpc_client_->getFileLockState (path.c_str());
        if (state == FILE_LOCKED_BY_ME_MANUAL || state == FILE_LOCKED_BY_ME_AUTO) {
            auto unlockFileAct { proxy_->createAction() };
            unlockFileAct->setText("解锁该文件");
            unlockFileAct->registerTriggered([this, path](DFMExtAction *, bool) {
                rpc_client_->unlockFile (path.c_str());
            });
            action->menu()->addAction(unlockFileAct);
        } else if (state == FILE_NOT_LOCKED) {
            auto lockFileAct { proxy_->createAction() };
            lockFileAct->setText("锁定该文件");
            lockFileAct->registerTriggered([this, path](DFMExtAction *, bool) {
                rpc_client_->lockFile (path.c_str());
            });
            action->menu()->addAction(lockFileAct);
        }

        auto shareLinkAct { proxy_->createAction() };
        shareLinkAct->setText("获取共享链接");
        shareLinkAct->registerTriggered([this, path](DFMExtAction *, bool) {
            rpc_client_->getShareLink (path.c_str());
        });

        auto internalLinkAct { proxy_->createAction() };
        internalLinkAct->setText("获取内部链接");
        internalLinkAct->registerTriggered([this, path](DFMExtAction *, bool) {
            rpc_client_->getInternalLink (path.c_str());
        });

        auto fileHistoryAct { proxy_->createAction() };
        fileHistoryAct->setText("查看文件历史");
        fileHistoryAct->registerTriggered([this, path](DFMExtAction *, bool) {
            rpc_client_->showFileHistory (path.c_str());
        });

        action->menu()->addAction(shareLinkAct);
        action->menu()->addAction(internalLinkAct);
        action->menu()->addAction(fileHistoryAct);
    });

    main->addAction(rootAction);
    return true;
}

bool SeaDriveMenuPlugin::buildEmptyAreaMenu(DFMExtMenu *main, const std::string &currentPath, bool onDesktop)
{
    return false;
}

}   // namespace SeaDrivePlugin
