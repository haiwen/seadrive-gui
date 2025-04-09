#include <dfm-extension/dfm-extension.h>

#include "seadrive-menu-plugin.h"
#include "seadrive-emblemicon-plugin.h"

static DFMEXT::DFMExtMenuPlugin *seadriveMenu { nullptr };
static DFMEXT::DFMExtEmblemIconPlugin *seadriveEmblemIcon { nullptr };

extern "C" void dfm_extension_initiliaze()
{
    seadriveMenu = new SeaDrivePlugin::SeaDriveMenuPlugin;
    seadriveEmblemIcon = new SeaDrivePlugin::SeaDriveEmblemIconPlugin;
}

extern "C" void dfm_extension_shutdown()
{
    delete seadriveMenu;
    delete seadriveEmblemIcon;
}

extern "C" DFMEXT::DFMExtMenuPlugin *dfm_extension_menu()
{
    return seadriveMenu;
}

extern "C" DFMEXT::DFMExtEmblemIconPlugin *dfm_extension_emblem()
{
    return seadriveEmblemIcon;
}
