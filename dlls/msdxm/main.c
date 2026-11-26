/*
 * Legacy Windows Media Player compatibility boundary
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS

#include <stdarg.h>
#include <stdlib.h>
#include "windows.h"
#include "ole2.h"
#include "initguid.h"
#include "wmp.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msdxm);

static const CLSID clsid_media_player = {0x22d6f312,0xb0f6,0x11d0,{0x94,0xab,0x00,0x80,0xc7,0x4c,0x7e,0x95}};
static LONG objects, factory_refs, server_locks;

typedef struct
{
    IDispatch IDispatch_iface;
    LONG ref;
    IWMPPlayer4 *backend;
} MediaPlayer;

/* Private late-binding identifiers only. The historical type library has
 * not been recovered; do not expose a legacy dual IID or register this
 * partial control until its native ABI and OLE hosting are implemented. */
enum legacy_member
{
    MEMBER_FILENAME = 0x10000,
    MEMBER_AUTOSTART,
    MEMBER_PLAY,
    MEMBER_PAUSE,
    MEMBER_STOP,
    MEMBER_OPEN
};

static const struct
{
    const WCHAR *name;
    DISPID id;
} members[] = {
    {L"FileName", MEMBER_FILENAME},
    {L"AutoStart", MEMBER_AUTOSTART},
    {L"Play", MEMBER_PLAY},
    {L"Pause", MEMBER_PAUSE},
    {L"Stop", MEMBER_STOP},
    {L"Open", MEMBER_OPEN},
};

static MediaPlayer *impl_from_IDispatch(IDispatch *iface)
{
    return CONTAINING_RECORD(iface, MediaPlayer, IDispatch_iface);
}

static HRESULT WINAPI player_QueryInterface(IDispatch *iface, REFIID iid, void **out)
{
    if(!out) return E_POINTER;
    *out = NULL;
    if(!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IDispatch))
        return E_NOINTERFACE;
    *out = iface;
    IDispatch_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI player_AddRef(IDispatch *iface)
{
    return InterlockedIncrement(&impl_from_IDispatch(iface)->ref);
}

static ULONG WINAPI player_Release(IDispatch *iface)
{
    MediaPlayer *player = impl_from_IDispatch(iface);
    LONG ref = InterlockedDecrement(&player->ref);

    if(!ref) {
        IWMPPlayer4_Release(player->backend);
        free(player);
        InterlockedDecrement(&objects);
    }
    return ref;
}

static HRESULT WINAPI player_GetTypeInfoCount(IDispatch *iface, UINT *count)
{
    if(!count) return E_POINTER;
    *count = 0;
    return S_OK;
}

static HRESULT WINAPI player_GetTypeInfo(IDispatch *iface, UINT index, LCID lcid, ITypeInfo **info)
{
    if(!info) return E_POINTER;
    *info = NULL;
    return DISP_E_BADINDEX;
}

static HRESULT WINAPI player_GetIDsOfNames(IDispatch *iface, REFIID iid, LPOLESTR *names,
        UINT count, LCID lcid, DISPID *ids)
{
    unsigned int i;

    if(!IsEqualGUID(iid, &IID_NULL)) return DISP_E_UNKNOWNINTERFACE;
    if(!names || !ids || !count || !names[0]) return E_INVALIDARG;
    for(i = 0; i < count; ++i) ids[i] = DISPID_UNKNOWN;
    if(count != 1) return DISP_E_UNKNOWNNAME;

    for(i = 0; i < ARRAY_SIZE(members); ++i) {
        if(!lstrcmpiW(names[0], members[i].name)) {
            ids[0] = members[i].id;
            return S_OK;
        }
    }
    return DISP_E_UNKNOWNNAME;
}

static HRESULT invoke_property(MediaPlayer *player, DISPID id, WORD flags, LCID lcid,
        DISPPARAMS *params, VARIANT *result, UINT *arg_error)
{
    IWMPSettings *settings = NULL;
    VARIANT value;
    HRESULT hr;

    if(flags == DISPATCH_PROPERTYGET) {
        if(params->cArgs || params->cNamedArgs) return DISP_E_BADPARAMCOUNT;
        if(!result) return E_POINTER;
        if(id == MEMBER_FILENAME) {
            V_VT(result) = VT_BSTR;
            V_BSTR(result) = NULL;
            hr = IWMPPlayer4_get_URL(player->backend, &V_BSTR(result));
        }else {
            hr = IWMPPlayer4_get_settings(player->backend, &settings);
            if(SUCCEEDED(hr)) {
                V_VT(result) = VT_BOOL;
                hr = IWMPSettings_get_autoStart(settings, &V_BOOL(result));
                IWMPSettings_Release(settings);
            }
        }
        if(FAILED(hr)) VariantClear(result);
        return hr;
    }

    if((id == MEMBER_OPEN && flags != DISPATCH_METHOD) ||
       (id != MEMBER_OPEN && flags != DISPATCH_PROPERTYPUT))
        return DISP_E_MEMBERNOTFOUND;
    if(params->cArgs != 1 || !params->rgvarg) return DISP_E_BADPARAMCOUNT;
    if(id == MEMBER_OPEN) {
        if(params->cNamedArgs) return DISP_E_NONAMEDARGS;
    }else if(params->cNamedArgs != 1 || !params->rgdispidNamedArgs ||
             params->rgdispidNamedArgs[0] != DISPID_PROPERTYPUT)
        return DISP_E_PARAMNOTFOUND;

    VariantInit(&value);
    hr = VariantChangeTypeEx(&value, &params->rgvarg[0], lcid, 0,
            id == MEMBER_AUTOSTART ? VT_BOOL : VT_BSTR);
    if(FAILED(hr)) {
        if(arg_error) *arg_error = 0;
        return hr;
    }

    if(id == MEMBER_AUTOSTART) {
        hr = IWMPPlayer4_get_settings(player->backend, &settings);
        if(SUCCEEDED(hr)) {
            hr = IWMPSettings_put_autoStart(settings, V_BOOL(&value));
            IWMPSettings_Release(settings);
        }
    }else {
        hr = IWMPPlayer4_put_URL(player->backend, V_BSTR(&value));
    }
    VariantClear(&value);
    return hr;
}

static HRESULT WINAPI player_Invoke(IDispatch *iface, DISPID id, REFIID iid, LCID lcid,
        WORD flags, DISPPARAMS *params, VARIANT *result, EXCEPINFO *exception, UINT *arg_error)
{
    MediaPlayer *player = impl_from_IDispatch(iface);
    IWMPControls *controls;
    HRESULT hr;

    if(!IsEqualGUID(iid, &IID_NULL)) return DISP_E_UNKNOWNINTERFACE;
    if(!params) return E_POINTER;
    if(result) VariantInit(result);

    switch(id) {
    case MEMBER_FILENAME:
    case MEMBER_AUTOSTART:
        return invoke_property(player, id, flags, lcid, params, result, arg_error);
    case MEMBER_OPEN:
        if(flags != DISPATCH_METHOD) return DISP_E_MEMBERNOTFOUND;
        return invoke_property(player, id, flags, lcid, params, result, arg_error);
    case MEMBER_PLAY:
    case MEMBER_PAUSE:
    case MEMBER_STOP:
        if(!(flags & DISPATCH_METHOD)) return DISP_E_MEMBERNOTFOUND;
        if(params->cArgs || params->cNamedArgs) return DISP_E_BADPARAMCOUNT;
        hr = IWMPPlayer4_get_controls(player->backend, &controls);
        if(FAILED(hr)) return hr;
        if(id == MEMBER_PLAY) hr = IWMPControls_play(controls);
        else if(id == MEMBER_PAUSE) hr = IWMPControls_pause(controls);
        else hr = IWMPControls_stop(controls);
        IWMPControls_Release(controls);
        return hr;
    default:
        return DISP_E_MEMBERNOTFOUND;
    }
}

static const IDispatchVtbl player_vtbl = {
    player_QueryInterface, player_AddRef, player_Release,
    player_GetTypeInfoCount, player_GetTypeInfo, player_GetIDsOfNames, player_Invoke
};

static HRESULT WINAPI factory_QueryInterface(IClassFactory *iface, REFIID iid, void **out)
{
    if(!out) return E_POINTER;
    *out = NULL;
    if(!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IClassFactory))
        return E_NOINTERFACE;
    *out = iface;
    IClassFactory_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI factory_AddRef(IClassFactory *iface)
{
    return InterlockedIncrement(&factory_refs);
}

static ULONG WINAPI factory_Release(IClassFactory *iface)
{
    return InterlockedDecrement(&factory_refs);
}

static HRESULT WINAPI factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID iid, void **out)
{
    MediaPlayer *player;
    HRESULT hr;

    if(!out) return E_POINTER;
    *out = NULL;
    if(outer) return CLASS_E_NOAGGREGATION;
    if(!IsEqualGUID(iid, &IID_IUnknown) && !IsEqualGUID(iid, &IID_IDispatch))
        return E_NOINTERFACE;
    if(!(player = calloc(1, sizeof(*player)))) return E_OUTOFMEMORY;
    hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWMPPlayer4, (void **)&player->backend);
    if(FAILED(hr)) {
        free(player);
        return hr;
    }
    player->IDispatch_iface.lpVtbl = &player_vtbl;
    player->ref = 1;
    InterlockedIncrement(&objects);
    *out = &player->IDispatch_iface;
    return S_OK;
}

static HRESULT WINAPI factory_LockServer(IClassFactory *iface, BOOL lock)
{
    LONG count;

    if(lock) InterlockedIncrement(&server_locks);
    else {
        do {
            count = InterlockedCompareExchange(&server_locks, 0, 0);
            if(!count) return E_UNEXPECTED;
        }while(InterlockedCompareExchange(&server_locks, count - 1, count) != count);
    }
    return S_OK;
}

static const IClassFactoryVtbl factory_vtbl = {
    factory_QueryInterface, factory_AddRef, factory_Release,
    factory_CreateInstance, factory_LockServer
};
static IClassFactory factory = {&factory_vtbl};

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    TRACE("(%s %s %p)\n", debugstr_guid(clsid), debugstr_guid(iid), out);
    if(!out) return E_POINTER;
    *out = NULL;
    if(!IsEqualGUID(clsid, &clsid_media_player)) return CLASS_E_CLASSNOTAVAILABLE;
    return IClassFactory_QueryInterface(&factory, iid, out);
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return objects || factory_refs || server_locks ? S_FALSE : S_OK;
}
