
#include "ext-common.h"
#include "class-factory.h"
#include "applet-connection.h"
#include "guids.h"
#include "log.h"
#include "ext-utils.h"

#include "shell-ext.h"

volatile LONG       g_cRefThisDll = 0;              ///< reference count of this DLL.
HINSTANCE           g_hmodThisDll = NULL;           ///< handle to this DLL itself.
int                 g_cAprInit = 0;
DWORD               g_langid;
DWORD               g_langTimeout = 0;
HINSTANCE           g_hResInst = NULL;

extern "C" int APIENTRY
DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /* lpReserved */)
{
    if (dwReason == DLL_PROCESS_ATTACH)
    {
        if (g_hmodThisDll == NULL)
        {
            // g_csGlobalCOMGuard.Init();
        }

        // Extension DLL one-time initialization
        g_hmodThisDll = hInstance;
        seafile::AppletConnection::instance()->prepare();
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
        // do not clean up memory here:
        // if an application doesn't release all COM objects
        // but still unloads the dll, cleaning up ourselves
        // will lead to crashes.
        // better to leak some memory than to crash other apps.
        // sometimes an application doesn't release all COM objects
        // but still unloads the dll.
        // in that case, we do it ourselves

        // g_csGlobalCOMGuard.Term();
    }
    return 1;   // ok
}

STDAPI DllCanUnloadNow(void)
{
    return (g_cRefThisDll == 0 ? S_OK : S_FALSE);
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppvOut)
{
    if (ppvOut == 0)
        return E_POINTER;
    *ppvOut = NULL;

    if (IsEqualIID(rclsid, CLSID_SEAFILE_SHELLEXT)) {
        ShellExtClassFactory *pcf = new ShellExtClassFactory;
        const HRESULT hr = pcf->QueryInterface(riid, ppvOut);
        if(FAILED(hr))
            delete pcf;
        return hr;
    }

    seafile::Status status = seafile::None;
    if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_SYNCING)) {
        status = seafile::Syncing;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_ERROR)) {
        status = seafile::Error;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_SYNCED)) {
        status = seafile::Synced;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_PARTIAL_SYNCED)) {
        status = seafile::PartialSynced;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_CLOUD)) {
        status = seafile::Cloud;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_READ_ONLY)) {
        status = seafile::ReadOnly;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_LOCKED_BY_OTHERS)) {
        status = seafile::LockedByOthers;
    } else if (IsEqualIID(rclsid, CLSID_SEAFILE_ICON_LOCKED_BY_ME)) {
        status = seafile::LockedByMe;
    }

    if (status != seafile::None) {
        ShellExtClassFactory *pcf = new ShellExtClassFactory(status);
        const HRESULT hr = pcf->QueryInterface(riid, ppvOut);
        if(FAILED(hr))
            delete pcf;
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}
