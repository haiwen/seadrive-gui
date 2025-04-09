#include "seadrive-menu-plugin.h"

#include <sys/types.h>
#include <sys/xattr.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cassert>
#include <iostream>
#include <filesystem>

#include <dfm-extension/menu/dfmextmenu.h>
#include <dfm-extension/menu/dfmextmenuproxy.h>
#include <dfm-extension/menu/dfmextaction.h>

namespace fs = std::filesystem;

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
    /*
    main->registerDeleted([memTest](DFMExtMenu *self) {
    });
    */

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

    if (pathList.front().rfind(rpc_client_->getSeaDriveDir(), 0) != 0) {
        return true;
    }

    auto rootAction { proxy_->createAction() };
    rootAction->setText("SeaDrive");

    auto menu { proxy_->createMenu() };

    rootAction->setMenu(menu);
    rootAction->registerHovered([this, pathList](DFMExtAction *action) {
        std::string path = pathList.front().substr(rpc_client_->getSeaDriveDir().size());
        if (!path.empty() && path[0] == '/') {
            path = path.substr(1);
        }
        if (!action->menu()->actions().empty())
            return;

        int inRepo = rpc_client_->isPathInRepo (path.c_str());
        if (inRepo < 0) {
            return;
        }

        if (fs::is_directory(path)) {
            auto uploadLinkAct { proxy_->createAction() };
            uploadLinkAct->setText("获取上传链接");
            uploadLinkAct->registerTriggered([this, pathList](DFMExtAction *, bool) {
                rpc_client_->getUploadLink (pathList.front().c_str());
            });
            action->menu()->addAction(uploadLinkAct);
            return;
        }

        int state = rpc_client_->getFileLockState (path.c_str());
        if (state == LOCKED) {
            auto unlockFileAct { proxy_->createAction() };
            unlockFileAct->setText("解锁该文件");
            unlockFileAct->registerTriggered([this, pathList](DFMExtAction *, bool) {
                rpc_client_->unlockFile (pathList.front().c_str());
            });
            action->menu()->addAction(unlockFileAct);
        } else if (state == UNLOCKED) {
            auto lockFileAct { proxy_->createAction() };
            lockFileAct->setText("锁定该文件");
            lockFileAct->registerTriggered([this, pathList](DFMExtAction *, bool) {
                rpc_client_->lockFile (pathList.front().c_str());
            });
            action->menu()->addAction(lockFileAct);
        }

        auto shareLinkAct { proxy_->createAction() };
        shareLinkAct->setText("获取共享链接");
        shareLinkAct->registerTriggered([this, pathList](DFMExtAction *, bool) {
            rpc_client_->getShareLink (pathList.front().c_str());
        });

        auto internalLinkAct { proxy_->createAction() };
        internalLinkAct->setText("获取内部链接");
        internalLinkAct->registerTriggered([this, pathList](DFMExtAction *, bool) {
            rpc_client_->getInternalLink (pathList.front().c_str());
        });

        auto fileHistoryAct { proxy_->createAction() };
        fileHistoryAct->setText("查看文件历史");
        fileHistoryAct->registerTriggered([this, pathList](DFMExtAction *, bool) {
            rpc_client_->showFileHistory (pathList.front().c_str());
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
