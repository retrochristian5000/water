/*
 * Internet Explorer 4 Welcome Tour compatibility component
 *
 * This is a clean-room implementation of the small automation component
 * hosted by IE4TOUR.DLL.  The historical HTML and artwork are intentionally
 * kept separate from this code.
 */

#define COBJMACROS

#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "objbase.h"
#include "oleauto.h"
#include "ocidl.h"
#include "objsafe.h"

#include "ie4tour.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ie4tour);

#define EXPLORER_TIPS_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Tips"
#define SHOW_IE4_VALUE    "ShowIE4"

struct runonce_checkbox
{
    IRunOnceCheckBox IRunOnceCheckBox_iface;
    IObjectWithSite IObjectWithSite_iface;
    IObjectSafety IObjectSafety_iface;
    LONG ref;
    IUnknown *site;
    DWORD safety;
};

static HINSTANCE ie4tour_instance;
static LONG module_refs;

static inline struct runonce_checkbox *impl_from_IRunOnceCheckBox(IRunOnceCheckBox *iface)
{
    return CONTAINING_RECORD(iface, struct runonce_checkbox, IRunOnceCheckBox_iface);
}

static inline struct runonce_checkbox *impl_from_IObjectWithSite(IObjectWithSite *iface)
{
    return CONTAINING_RECORD(iface, struct runonce_checkbox, IObjectWithSite_iface);
}

static inline struct runonce_checkbox *impl_from_IObjectSafety(IObjectSafety *iface)
{
    return CONTAINING_RECORD(iface, struct runonce_checkbox, IObjectSafety_iface);
}

static ULONG runonce_addref(struct runonce_checkbox *This)
{
    return InterlockedIncrement(&This->ref);
}

static ULONG runonce_release(struct runonce_checkbox *This)
{
    ULONG ref = InterlockedDecrement(&This->ref);

    if (!ref)
    {
        if (This->site) IUnknown_Release(This->site);
        free(This);
        InterlockedDecrement(&module_refs);
    }
    return ref;
}

static HRESULT runonce_query_interface(struct runonce_checkbox *This, REFIID riid, void **obj)
{
    if (!obj) return E_POINTER;
    *obj = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) ||
        IsEqualIID(riid, &IID_IDispatch) ||
        IsEqualIID(riid, &IID_IRunOnceCheckBox))
        *obj = &This->IRunOnceCheckBox_iface;
    else if (IsEqualIID(riid, &IID_IObjectWithSite))
        *obj = &This->IObjectWithSite_iface;
    else if (IsEqualIID(riid, &IID_IObjectSafety))
        *obj = &This->IObjectSafety_iface;
    else
        return E_NOINTERFACE;

    runonce_addref(This);
    return S_OK;
}

static HRESULT WINAPI runonce_QueryInterface(IRunOnceCheckBox *iface, REFIID riid, void **obj)
{
    return runonce_query_interface(impl_from_IRunOnceCheckBox(iface), riid, obj);
}

static ULONG WINAPI runonce_AddRef(IRunOnceCheckBox *iface)
{
    return runonce_addref(impl_from_IRunOnceCheckBox(iface));
}

static ULONG WINAPI runonce_Release(IRunOnceCheckBox *iface)
{
    return runonce_release(impl_from_IRunOnceCheckBox(iface));
}

static HRESULT load_runonce_typeinfo(ITypeInfo **typeinfo)
{
    ITypeLib *typelib;
    WCHAR path[MAX_PATH];
    HRESULT hr;

    if (!typeinfo) return E_POINTER;
    *typeinfo = NULL;

    if (!GetModuleFileNameW(ie4tour_instance, path, ARRAY_SIZE(path)))
        return HRESULT_FROM_WIN32(GetLastError());

    hr = LoadTypeLibEx(path, REGKIND_NONE, &typelib);
    if (FAILED(hr)) return hr;

    hr = ITypeLib_GetTypeInfoOfGuid(typelib, &IID_IRunOnceCheckBox, typeinfo);
    ITypeLib_Release(typelib);
    return hr;
}

static HRESULT WINAPI runonce_GetTypeInfoCount(IRunOnceCheckBox *iface, UINT *count)
{
    if (!count) return E_POINTER;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI runonce_GetTypeInfo(IRunOnceCheckBox *iface, UINT index, LCID lcid,
                                           ITypeInfo **typeinfo)
{
    if (index) return DISP_E_BADINDEX;
    return load_runonce_typeinfo(typeinfo);
}

static HRESULT WINAPI runonce_GetIDsOfNames(IRunOnceCheckBox *iface, REFIID riid,
                                             LPOLESTR *names, UINT count, LCID lcid,
                                             DISPID *dispids)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    if (!IsEqualIID(riid, &IID_NULL)) return DISP_E_UNKNOWNINTERFACE;

    hr = load_runonce_typeinfo(&typeinfo);
    if (FAILED(hr)) return hr;

    hr = ITypeInfo_GetIDsOfNames(typeinfo, names, count, dispids);
    ITypeInfo_Release(typeinfo);
    return hr;
}

static HRESULT WINAPI runonce_Invoke(IRunOnceCheckBox *iface, DISPID dispid, REFIID riid,
                                      LCID lcid, WORD flags, DISPPARAMS *params,
                                      VARIANT *result, EXCEPINFO *excep, UINT *argerr)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    if (!IsEqualIID(riid, &IID_NULL)) return DISP_E_UNKNOWNINTERFACE;

    hr = load_runonce_typeinfo(&typeinfo);
    if (FAILED(hr)) return hr;

    hr = ITypeInfo_Invoke(typeinfo, iface, dispid, flags, params, result, excep, argerr);
    ITypeInfo_Release(typeinfo);
    return hr;
}

static HRESULT WINAPI runonce_get_ShowIE4State(IRunOnceCheckBox *iface, BOOL *value)
{
    HKEY key;
    DWORD data = 0, type = 0, size = sizeof(data);
    LSTATUS status;

    if (!value) return E_POINTER;
    *value = FALSE;

    status = RegOpenKeyExA(HKEY_CURRENT_USER, EXPLORER_TIPS_KEY, 0, KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS) return S_OK;

    status = RegQueryValueExA(key, SHOW_IE4_VALUE, NULL, &type, (BYTE *)&data, &size);
    RegCloseKey(key);

    if (status == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(data))
        *value = data != 0;

    TRACE("ShowIE4State -> %u\n", *value);
    return S_OK;
}

static HRESULT WINAPI runonce_put_ShowIE4State(IRunOnceCheckBox *iface, BOOL value)
{
    HKEY key;
    DWORD data = !!value;
    LSTATUS status;

    status = RegCreateKeyExA(HKEY_CURRENT_USER, EXPLORER_TIPS_KEY, 0, NULL, 0,
                             KEY_SET_VALUE, NULL, &key, NULL);
    if (status != ERROR_SUCCESS) return HRESULT_FROM_WIN32(status);

    status = RegSetValueExA(key, SHOW_IE4_VALUE, 0, REG_DWORD,
                            (const BYTE *)&data, sizeof(data));
    RegCloseKey(key);

    TRACE("ShowIE4State <- %u\n", data);
    return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
}

static const IRunOnceCheckBoxVtbl runonce_vtbl =
{
    runonce_QueryInterface,
    runonce_AddRef,
    runonce_Release,
    runonce_GetTypeInfoCount,
    runonce_GetTypeInfo,
    runonce_GetIDsOfNames,
    runonce_Invoke,
    runonce_get_ShowIE4State,
    runonce_put_ShowIE4State
};

static HRESULT WINAPI objectsite_QueryInterface(IObjectWithSite *iface, REFIID riid, void **obj)
{
    return runonce_query_interface(impl_from_IObjectWithSite(iface), riid, obj);
}

static ULONG WINAPI objectsite_AddRef(IObjectWithSite *iface)
{
    return runonce_addref(impl_from_IObjectWithSite(iface));
}

static ULONG WINAPI objectsite_Release(IObjectWithSite *iface)
{
    return runonce_release(impl_from_IObjectWithSite(iface));
}

static HRESULT WINAPI objectsite_SetSite(IObjectWithSite *iface, IUnknown *site)
{
    struct runonce_checkbox *This = impl_from_IObjectWithSite(iface);

    if (site) IUnknown_AddRef(site);
    if (This->site) IUnknown_Release(This->site);
    This->site = site;
    return S_OK;
}

static HRESULT WINAPI objectsite_GetSite(IObjectWithSite *iface, REFIID riid, void **obj)
{
    struct runonce_checkbox *This = impl_from_IObjectWithSite(iface);

    if (!obj) return E_POINTER;
    *obj = NULL;
    return This->site ? IUnknown_QueryInterface(This->site, riid, obj) : E_FAIL;
}

static const IObjectWithSiteVtbl objectsite_vtbl =
{
    objectsite_QueryInterface,
    objectsite_AddRef,
    objectsite_Release,
    objectsite_SetSite,
    objectsite_GetSite
};

static HRESULT WINAPI objectsafety_QueryInterface(IObjectSafety *iface, REFIID riid, void **obj)
{
    return runonce_query_interface(impl_from_IObjectSafety(iface), riid, obj);
}

static ULONG WINAPI objectsafety_AddRef(IObjectSafety *iface)
{
    return runonce_addref(impl_from_IObjectSafety(iface));
}

static ULONG WINAPI objectsafety_Release(IObjectSafety *iface)
{
    return runonce_release(impl_from_IObjectSafety(iface));
}

static HRESULT WINAPI objectsafety_GetInterfaceSafetyOptions(IObjectSafety *iface, REFIID riid,
                                                              DWORD *supported, DWORD *enabled)
{
    struct runonce_checkbox *This = impl_from_IObjectSafety(iface);
    const DWORD mask = INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA;

    if (!supported || !enabled) return E_POINTER;
    if (!IsEqualIID(riid, &IID_IDispatch) && !IsEqualIID(riid, &IID_IRunOnceCheckBox))
        return E_NOINTERFACE;

    *supported = mask;
    *enabled = This->safety & mask;
    return S_OK;
}

static HRESULT WINAPI objectsafety_SetInterfaceSafetyOptions(IObjectSafety *iface, REFIID riid,
                                                              DWORD mask, DWORD enabled)
{
    struct runonce_checkbox *This = impl_from_IObjectSafety(iface);
    const DWORD supported = INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA;

    if (!IsEqualIID(riid, &IID_IDispatch) && !IsEqualIID(riid, &IID_IRunOnceCheckBox))
        return E_NOINTERFACE;
    if ((mask & enabled) & ~supported) return E_FAIL;

    This->safety = (This->safety & ~mask) | (enabled & mask & supported);
    return S_OK;
}

static const IObjectSafetyVtbl objectsafety_vtbl =
{
    objectsafety_QueryInterface,
    objectsafety_AddRef,
    objectsafety_Release,
    objectsafety_GetInterfaceSafetyOptions,
    objectsafety_SetInterfaceSafetyOptions
};

static HRESULT create_runonce(REFIID riid, void **obj)
{
    struct runonce_checkbox *This;
    HRESULT hr;

    if (!(This = calloc(1, sizeof(*This)))) return E_OUTOFMEMORY;

    This->IRunOnceCheckBox_iface.lpVtbl = &runonce_vtbl;
    This->IObjectWithSite_iface.lpVtbl = &objectsite_vtbl;
    This->IObjectSafety_iface.lpVtbl = &objectsafety_vtbl;
    This->ref = 1;
    This->safety = INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA;
    InterlockedIncrement(&module_refs);

    hr = runonce_query_interface(This, riid, obj);
    runonce_release(This);
    return hr;
}

static HRESULT WINAPI factory_QueryInterface(IClassFactory *iface, REFIID riid, void **obj)
{
    if (!obj) return E_POINTER;
    *obj = NULL;

    if (!IsEqualIID(riid, &IID_IUnknown) && !IsEqualIID(riid, &IID_IClassFactory))
        return E_NOINTERFACE;

    *obj = iface;
    IClassFactory_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI factory_AddRef(IClassFactory *iface)
{
    InterlockedIncrement(&module_refs);
    return 2;
}

static ULONG WINAPI factory_Release(IClassFactory *iface)
{
    InterlockedDecrement(&module_refs);
    return 1;
}

static HRESULT WINAPI factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
                                              REFIID riid, void **obj)
{
    if (outer)
    {
        if (obj) *obj = NULL;
        return CLASS_E_NOAGGREGATION;
    }
    return create_runonce(riid, obj);
}

static HRESULT WINAPI factory_LockServer(IClassFactory *iface, BOOL lock)
{
    if (lock) InterlockedIncrement(&module_refs);
    else InterlockedDecrement(&module_refs);
    return S_OK;
}

static const IClassFactoryVtbl factory_vtbl =
{
    factory_QueryInterface,
    factory_AddRef,
    factory_Release,
    factory_CreateInstance,
    factory_LockServer
};

static IClassFactory runonce_factory = { &factory_vtbl };

/***********************************************************************
 *           DllGetClassObject   (IE4TOUR.2)
 */
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **obj)
{
    TRACE("(%s, %s, %p)\n", debugstr_guid(clsid), debugstr_guid(riid), obj);

    if (!IsEqualCLSID(clsid, &CLSID_RunOnceCheckBox))
    {
        if (obj) *obj = NULL;
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    return IClassFactory_QueryInterface(&runonce_factory, riid, obj);
}

/***********************************************************************
 *           DllCanUnloadNow     (IE4TOUR.1)
 */
HRESULT WINAPI DllCanUnloadNow(void)
{
    return module_refs ? S_FALSE : S_OK;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        ie4tour_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
