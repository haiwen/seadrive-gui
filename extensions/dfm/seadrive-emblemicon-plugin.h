#ifndef SEADRIVEEMBLEMICONPLUGIN_H
#define SEADRIVEEMBLEMICONPLUGIN_H

#include <dfm-extension/emblemicon/dfmextemblemiconplugin.h>
#include "rpc-client.h"

namespace SeaDrivePlugin {

class SeaDriveEmblemIconPlugin : public DFMEXT::DFMExtEmblemIconPlugin
{
public:
    SeaDriveEmblemIconPlugin();
    ~SeaDriveEmblemIconPlugin();

    DFMEXT::DFMExtEmblem locationEmblemIcons(const std::string &filePath, int systemIconCount) const DFM_FAKE_OVERRIDE;

private:
    SeaDriveRpcClient *rpc_client_;
};

}   // namespace SeaDrivePlugin

#endif   // SEADRIVEEMBLEMICONPLUGIN_H
