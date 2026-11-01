/*
 * Copyright 2011 Jacek Caban for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "ieframe.h"

#include "initguid.h"
#include "rpcproxy.h"
#include "shlguid.h"
#include "isguids.h"
#include "ieautomation.h"
#include "shlwapi.h"
#include "winver.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ieframe);

LONG module_ref = 0;
HINSTANCE ieframe_instance;

static ITypeLib *typelib;
static ITypeInfo *typeinfos[LAST_tid];

static REFIID tid_ids[] = {
#define XIID(iface) &IID_ ## iface,
#define XCLSID(class) &CLSID_ ## class,
TID_LIST
#undef XIID
#undef XCLSID
};

static HRESULT load_typelib(void)
{
    HRESULT hres;
    ITypeLib *tl;

    hres = LoadRegTypeLib(&LIBID_SHDocVw, 1, 1, LOCALE_SYSTEM_DEFAULT, &tl);
    if(FAILED(hres)) {
        ERR("LoadRegTypeLib failed: %08lx\n", hres);
        return hres;
    }

    if(InterlockedCompareExchangePointer((void**)&typelib, tl, NULL))
        ITypeLib_Release(tl);
    return hres;
}

HRESULT get_typeinfo(tid_t tid, ITypeInfo **typeinfo)
{
    HRESULT hres;

    if(!typelib)
        hres = load_typelib();
    if(!typelib)
        return hres;

    if(!typeinfos[tid]) {
        ITypeInfo *ti;

        hres = ITypeLib_GetTypeInfoOfGuid(typelib, tid_ids[tid], &ti);
        if(FAILED(hres)) {
            ERR("GetTypeInfoOfGuid(%s) failed: %08lx\n", debugstr_guid(tid_ids[tid]), hres);
            return hres;
        }

        if(InterlockedCompareExchangePointer((void**)(typeinfos+tid), ti, NULL))
            ITypeInfo_Release(ti);
    }

    *typeinfo = typeinfos[tid];
    return S_OK;
}

static void release_typelib(void)
{
    unsigned i;

    if(!typelib)
        return;

    for(i=0; i < ARRAY_SIZE(typeinfos); i++) {
        if(typeinfos[i])
            ITypeInfo_Release(typeinfos[i]);
    }

    ITypeLib_Release(typelib);
}

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
    *ppv = NULL;

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", iface, ppv);
        *ppv = iface;
    }else if(IsEqualGUID(&IID_IClassFactory, riid)) {
        TRACE("(%p)->(IID_IClassFactory %p)\n", iface, ppv);
        *ppv = iface;
    }

    if(*ppv) {
        IUnknown_AddRef((IUnknown*)*ppv);
        return S_OK;
    }

    FIXME("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppv);
    return E_NOINTERFACE;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    TRACE("(%p)\n", iface);
    return 2;
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    TRACE("(%p)\n", iface);
    return 1;
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL fLock)
{
    TRACE("(%p)->(%x)\n", iface, fLock);
    return S_OK;
}

static const IClassFactoryVtbl WebBrowserFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    WebBrowser_Create,
    ClassFactory_LockServer
};

static IClassFactory WebBrowserFactory = { &WebBrowserFactoryVtbl };

static const IClassFactoryVtbl WebBrowserV1FactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    WebBrowserV1_Create,
    ClassFactory_LockServer
};

static IClassFactory WebBrowserV1Factory = { &WebBrowserV1FactoryVtbl };

static const IClassFactoryVtbl InternetShortcutFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    InternetShortcut_Create,
    ClassFactory_LockServer
};

static IClassFactory InternetShortcutFactory = { &InternetShortcutFactoryVtbl };

static const IClassFactoryVtbl CUrlHistoryFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    CUrlHistory_Create,
    ClassFactory_LockServer
};

static IClassFactory CUrlHistoryFactory = { &CUrlHistoryFactoryVtbl };

/******************************************************************
 *              DllMain (ieframe.@)
 */
BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpv)
{
    TRACE("(%p %ld %p)\n", hInstDLL, fdwReason, lpv);

    switch(fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        ieframe_instance = hInstDLL;
        register_iewindow_class();
        DisableThreadLibraryCalls(ieframe_instance);
        break;
    case DLL_PROCESS_DETACH:
        if (lpv) break;
        unregister_iewindow_class();
        release_typelib();
    }

    return TRUE;
}

/***********************************************************************
 *		DllGetClassObject	(ieframe.@)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    if(IsEqualGUID(&CLSID_WebBrowser, rclsid)) {
        TRACE("(CLSID_WebBrowser %s %p)\n", debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&WebBrowserFactory, riid, ppv);
    }

    if(IsEqualGUID(&CLSID_WebBrowser_V1, rclsid)) {
        TRACE("(CLSID_WebBrowser_V1 %s %p)\n", debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&WebBrowserV1Factory, riid, ppv);
    }

    if(IsEqualGUID(rclsid, &CLSID_InternetShortcut)) {
        TRACE("(CLSID_InternetShortcut %s %p)\n", debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&InternetShortcutFactory, riid, ppv);
    }

    if(IsEqualGUID(&CLSID_CUrlHistory, rclsid)) {
        TRACE("(CLSID_CUrlHistory %s %p)\n", debugstr_guid(riid), ppv);
        return IClassFactory_QueryInterface(&CUrlHistoryFactory, riid, ppv);
    }

    FIXME("%s %s %p\n", debugstr_guid(rclsid), debugstr_guid(riid), ppv);
    return CLASS_E_CLASSNOTAVAILABLE;
}

static const IClassFactoryVtbl InternetExplorerFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    InternetExplorer_Create,
    ClassFactory_LockServer
};

IClassFactory InternetExplorerFactory = { &InternetExplorerFactoryVtbl };

static const IClassFactoryVtbl InternetExplorerManagerFactoryVtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    InternetExplorerManager_Create,
    ClassFactory_LockServer
};

IClassFactory InternetExplorerManagerFactory = { &InternetExplorerManagerFactoryVtbl };

/***********************************************************************
 *          DllCanUnloadNow (ieframe.@)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    TRACE("()\n");
    return module_ref ? S_FALSE : S_OK;
}


/***********************************************************************
 *              DllGetVersion (ieframe.@)
 *
 * Read IEFRAME's own file version rather than inferring it from the OS.
 */
HRESULT WINAPI DllGetVersion(DLLVERSIONINFO *info)
{
    WCHAR path[MAX_PATH];
    DWORD path_len, version_len, dummy = 0;
    VS_FIXEDFILEINFO *fixed;
    UINT fixed_len;
    void *data;
    HRESULT hr = E_FAIL;

    if (!info)
        return E_POINTER;
    if (info->cbSize != sizeof(*info) && info->cbSize != sizeof(DLLVERSIONINFO2))
        return E_INVALIDARG;

    path_len = GetModuleFileNameW(ieframe_instance, path, ARRAY_SIZE(path));
    if (!path_len || path_len >= ARRAY_SIZE(path))
        return E_FAIL;

    version_len = GetFileVersionInfoSizeW(path, &dummy);
    if (!version_len)
        return E_FAIL;
    if (!(data = malloc(version_len)))
        return E_OUTOFMEMORY;

    if (GetFileVersionInfoW(path, 0, version_len, data)
            && VerQueryValueW(data, L"\\", (void **)&fixed, &fixed_len)
            && fixed_len >= sizeof(*fixed) && fixed->dwSignature == 0xfeef04bd)
    {
        info->dwMajorVersion = HIWORD(fixed->dwFileVersionMS);
        info->dwMinorVersion = LOWORD(fixed->dwFileVersionMS);
        info->dwBuildNumber = HIWORD(fixed->dwFileVersionLS);
        info->dwPlatformID = DLLVER_PLATFORM_NT;

        if (info->cbSize == sizeof(DLLVERSIONINFO2))
        {
            DLLVERSIONINFO2 *info2 = (DLLVERSIONINFO2 *)info;

            info2->dwFlags = 0;
            info2->ullVersion = MAKEDLLVERULL(info->dwMajorVersion, info->dwMinorVersion,
                    info->dwBuildNumber, LOWORD(fixed->dwFileVersionLS));
        }
        hr = S_OK;
    }

    free(data);
    return hr;
}

/***********************************************************************
 *          IEGetWriteableHKCU (ieframe.@)
 */
HRESULT WINAPI IEGetWriteableHKCU(HKEY *pkey)
{
    FIXME("(%p) stub\n", pkey);
    return E_NOTIMPL;
}

/***********************************************************************
 *          SetQueryNetSessionCount (ieframe.@)
 */
LONG WINAPI SetQueryNetSessionCount(DWORD session_op)
{
    static LONG session_count;

    TRACE("(%lx)\n", session_op);

    switch(session_op)
    {
    case SESSION_QUERY:
        return session_count;
    case SESSION_INCREMENT:
        return InterlockedIncrement(&session_count);
    case SESSION_DECREMENT:
        return InterlockedDecrement(&session_count);
    };

    return 0;
}
