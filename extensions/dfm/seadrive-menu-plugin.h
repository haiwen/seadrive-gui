#ifndef SEADRIVEMENUPLUGIN_H
#define SEADRIVEMENUPLUGIN_H

#include <dfm-extension/menu/dfmextmenuplugin.h>
#include "rpc-client.h"

namespace SeaDrivePlugin {

class SeaDriveMenuPlugin : public DFMEXT::DFMExtMenuPlugin
{
public:
    SeaDriveMenuPlugin();
    ~SeaDriveMenuPlugin();

    void initialize(DFMEXT::DFMExtMenuProxy *proxy) DFM_FAKE_OVERRIDE;
    bool buildNormalMenu(DFMEXT::DFMExtMenu *main,
                         const std::string &currentPath,
                         const std::string &focusPath,
                         const std::list<std::string> &pathList,
                         bool onDesktop) DFM_FAKE_OVERRIDE;
    bool buildEmptyAreaMenu(DFMEXT::DFMExtMenu *main, const std::string &currentPath, bool onDesktop) DFM_FAKE_OVERRIDE;

private:
    SeaDriveRpcClient *rpc_client_;
    DFMEXT::DFMExtMenuProxy *proxy_ { nullptr };
};

}   // namespace SeaDrivePlugin

#endif   // SEADRIVEMENUPLUGIN_H
