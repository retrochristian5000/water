/*
 * Shockwave Flash 5 compatibility control
 *
 * This is a clean-room compatibility implementation.  It provides the
 * historical COM/ActiveX surface but does not contain a Flash renderer.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "ole2.h"
#include "oleauto.h"
#include "olectl.h"
#include "initguid.h"
#include "swflash.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(swflash);

#define SWFLASH_VERSION 0x0005002c
#define SWFLASH_MISC_STATUS 131473

struct swflash
{
    IShockwaveFlash IShockwaveFlash_iface;
    IOleObject IOleObject_iface;
    IPersistStreamInit IPersistStreamInit_iface;
    IOleControl IOleControl_iface;
    LONG ref;

    IOleClientSite *client_site;
    SIZEL extent;
    BOOL dirty;

    VARIANT_BOOL playing;
    int quality;
    int scale_mode;
    int align_mode;
    LONG background_color;
    VARIANT_BOOL loop;
    BSTR movie;
    LONG frame_num;
    BSTR wmode;
    BSTR salign;
    VARIANT_BOOL menu;
    BSTR base;
    BSTR scale;
    VARIANT_BOOL device_font;
    VARIANT_BOOL embed_movie;
    BSTR bgcolor;
    BSTR quality2;
    BSTR swremote;
};

struct class_factory
{
    IClassFactory IClassFactory_iface;
    HRESULT (*create)(IUnknown *outer, REFIID iid, void **out);
};

struct flashprop
{
    IPropertyPage IPropertyPage_iface;
    LONG ref;
    IPropertyPageSite *site;
};

static HINSTANCE instance;
static ITypeInfo *shockwave_typeinfo;
static LONG object_count;
static LONG factory_refs;
static LONG server_locks;

static inline struct swflash *impl_from_IShockwaveFlash(IShockwaveFlash *iface)
{
    return CONTAINING_RECORD(iface, struct swflash, IShockwaveFlash_iface);
}

static inline struct swflash *impl_from_IOleObject(IOleObject *iface)
{
    return CONTAINING_RECORD(iface, struct swflash, IOleObject_iface);
}

static inline struct swflash *impl_from_IPersistStreamInit(IPersistStreamInit *iface)
{
    return CONTAINING_RECORD(iface, struct swflash, IPersistStreamInit_iface);
}

static inline struct swflash *impl_from_IOleControl(IOleControl *iface)
{
    return CONTAINING_RECORD(iface, struct swflash, IOleControl_iface);
}

static inline struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static inline struct flashprop *impl_from_IPropertyPage(IPropertyPage *iface)
{
    return CONTAINING_RECORD(iface, struct flashprop, IPropertyPage_iface);
}

static HRESULT get_shockwave_typeinfo(ITypeInfo **out)
{
    WCHAR path[MAX_PATH];
    ITypeLib *typelib;
    ITypeInfo *typeinfo;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;

    if (!shockwave_typeinfo)
    {
        if (!GetModuleFileNameW(instance, path, ARRAY_SIZE(path)))
            return HRESULT_FROM_WIN32(GetLastError());

        hr = LoadTypeLibEx(path, REGKIND_NONE, &typelib);
        if (FAILED(hr)) return hr;

        hr = ITypeLib_GetTypeInfoOfGuid(typelib, &IID_IShockwaveFlash, &typeinfo);
        ITypeLib_Release(typelib);
        if (FAILED(hr)) return hr;

        if (InterlockedCompareExchangePointer((void **)&shockwave_typeinfo, typeinfo, NULL))
            ITypeInfo_Release(typeinfo);
    }

    *out = shockwave_typeinfo;
    ITypeInfo_AddRef(*out);
    return S_OK;
}

static HRESULT swflash_query_interface(struct swflash *This, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IDispatch)
        || IsEqualIID(iid, &IID_IShockwaveFlash))
        *out = &This->IShockwaveFlash_iface;
    else if (IsEqualIID(iid, &IID_IOleObject))
        *out = &This->IOleObject_iface;
    else if (IsEqualIID(iid, &IID_IPersist) || IsEqualIID(iid, &IID_IPersistStream)
             || IsEqualIID(iid, &IID_IPersistStreamInit))
        *out = &This->IPersistStreamInit_iface;
    else if (IsEqualIID(iid, &IID_IOleControl))
        *out = &This->IOleControl_iface;
    else
        return E_NOINTERFACE;

    InterlockedIncrement(&This->ref);
    return S_OK;
}

static ULONG swflash_addref(struct swflash *This)
{
    return InterlockedIncrement(&This->ref);
}

static ULONG swflash_release(struct swflash *This)
{
    ULONG ref = InterlockedDecrement(&This->ref);

    if (!ref)
    {
        if (This->client_site) IOleClientSite_Release(This->client_site);
        SysFreeString(This->movie);
        SysFreeString(This->wmode);
        SysFreeString(This->salign);
        SysFreeString(This->base);
        SysFreeString(This->scale);
        SysFreeString(This->bgcolor);
        SysFreeString(This->quality2);
        SysFreeString(This->swremote);
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&object_count);
    }
    return ref;
}

static HRESULT set_bstr(struct swflash *This, BSTR *slot, BSTR value)
{
    BSTR copy = NULL;

    if (value && !(copy = SysAllocString(value))) return E_OUTOFMEMORY;
    SysFreeString(*slot);
    *slot = copy;
    This->dirty = TRUE;
    return S_OK;
}

static HRESULT get_bstr(BSTR value, BSTR *out)
{
    if (!out) return E_POINTER;
    if (!(*out = SysAllocString(value ? value : L""))) return E_OUTOFMEMORY;
    return S_OK;
}

/* IShockwaveFlash / IDispatch */

static HRESULT WINAPI ShockwaveFlash_QueryInterface(IShockwaveFlash *iface, REFIID iid, void **out)
{
    return swflash_query_interface(impl_from_IShockwaveFlash(iface), iid, out);
}

static ULONG WINAPI ShockwaveFlash_AddRef(IShockwaveFlash *iface)
{
    return swflash_addref(impl_from_IShockwaveFlash(iface));
}

static ULONG WINAPI ShockwaveFlash_Release(IShockwaveFlash *iface)
{
    return swflash_release(impl_from_IShockwaveFlash(iface));
}

static HRESULT WINAPI ShockwaveFlash_GetTypeInfoCount(IShockwaveFlash *iface, UINT *count)
{
    if (!count) return E_POINTER;
    *count = 1;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_GetTypeInfo(IShockwaveFlash *iface, UINT index, LCID lcid,
                                                  ITypeInfo **out)
{
    if (index) return DISP_E_BADINDEX;
    return get_shockwave_typeinfo(out);
}

static HRESULT WINAPI ShockwaveFlash_GetIDsOfNames(IShockwaveFlash *iface, REFIID iid,
                                                    LPOLESTR *names, UINT count, LCID lcid,
                                                    DISPID *dispids)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    if (!IsEqualIID(iid, &IID_NULL)) return DISP_E_UNKNOWNINTERFACE;
    hr = get_shockwave_typeinfo(&typeinfo);
    if (FAILED(hr)) return hr;
    hr = DispGetIDsOfNames(typeinfo, names, count, dispids);
    ITypeInfo_Release(typeinfo);
    return hr;
}

static HRESULT WINAPI ShockwaveFlash_Invoke(IShockwaveFlash *iface, DISPID dispid, REFIID iid,
                                             LCID lcid, WORD flags, DISPPARAMS *params,
                                             VARIANT *result, EXCEPINFO *exception, UINT *argerr)
{
    ITypeInfo *typeinfo;
    HRESULT hr;

    if (!IsEqualIID(iid, &IID_NULL)) return DISP_E_UNKNOWNINTERFACE;
    hr = get_shockwave_typeinfo(&typeinfo);
    if (FAILED(hr)) return hr;
    hr = DispInvoke(iface, typeinfo, dispid, flags, params, result, exception, argerr);
    ITypeInfo_Release(typeinfo);
    return hr;
}

#define DEFINE_LONG_GETTER(name, field) \
static HRESULT WINAPI ShockwaveFlash_get_##name(IShockwaveFlash *iface, LONG *value) \
{ \
    if (!value) return E_POINTER; \
    *value = impl_from_IShockwaveFlash(iface)->field; \
    return S_OK; \
}

#define DEFINE_LONG_SETTER(name, field) \
static HRESULT WINAPI ShockwaveFlash_put_##name(IShockwaveFlash *iface, LONG value) \
{ \
    struct swflash *This = impl_from_IShockwaveFlash(iface); \
    This->field = value; \
    This->dirty = TRUE; \
    return S_OK; \
}

#define DEFINE_INT_GETTER(name, field) \
static HRESULT WINAPI ShockwaveFlash_get_##name(IShockwaveFlash *iface, int *value) \
{ \
    if (!value) return E_POINTER; \
    *value = impl_from_IShockwaveFlash(iface)->field; \
    return S_OK; \
}

#define DEFINE_INT_SETTER(name, field) \
static HRESULT WINAPI ShockwaveFlash_put_##name(IShockwaveFlash *iface, int value) \
{ \
    struct swflash *This = impl_from_IShockwaveFlash(iface); \
    This->field = value; \
    This->dirty = TRUE; \
    return S_OK; \
}

#define DEFINE_BOOL_GETTER(name, field) \
static HRESULT WINAPI ShockwaveFlash_get_##name(IShockwaveFlash *iface, VARIANT_BOOL *value) \
{ \
    if (!value) return E_POINTER; \
    *value = impl_from_IShockwaveFlash(iface)->field; \
    return S_OK; \
}

#define DEFINE_BOOL_SETTER(name, field) \
static HRESULT WINAPI ShockwaveFlash_put_##name(IShockwaveFlash *iface, VARIANT_BOOL value) \
{ \
    struct swflash *This = impl_from_IShockwaveFlash(iface); \
    This->field = value ? VARIANT_TRUE : VARIANT_FALSE; \
    This->dirty = TRUE; \
    return S_OK; \
}

#define DEFINE_BSTR_PROPERTY(name, field) \
static HRESULT WINAPI ShockwaveFlash_get_##name(IShockwaveFlash *iface, BSTR *value) \
{ \
    return get_bstr(impl_from_IShockwaveFlash(iface)->field, value); \
} \
static HRESULT WINAPI ShockwaveFlash_put_##name(IShockwaveFlash *iface, BSTR value) \
{ \
    struct swflash *This = impl_from_IShockwaveFlash(iface); \
    return set_bstr(This, &This->field, value); \
}

static HRESULT WINAPI ShockwaveFlash_get_ReadyState(IShockwaveFlash *iface, LONG *value)
{
    if (!value) return E_POINTER;
    *value = 4; /* complete: the compatibility control itself is ready */
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_get_TotalFrames(IShockwaveFlash *iface, LONG *value)
{
    if (!value) return E_POINTER;
    *value = 0;
    return S_OK;
}

DEFINE_BOOL_GETTER(Playing, playing)
DEFINE_BOOL_SETTER(Playing, playing)
DEFINE_INT_GETTER(Quality, quality)
DEFINE_INT_SETTER(Quality, quality)
DEFINE_INT_GETTER(ScaleMode, scale_mode)
DEFINE_INT_SETTER(ScaleMode, scale_mode)
DEFINE_INT_GETTER(AlignMode, align_mode)
DEFINE_INT_SETTER(AlignMode, align_mode)
DEFINE_LONG_GETTER(BackgroundColor, background_color)
DEFINE_LONG_SETTER(BackgroundColor, background_color)
DEFINE_BOOL_GETTER(Loop, loop)
DEFINE_BOOL_SETTER(Loop, loop)
DEFINE_BSTR_PROPERTY(Movie, movie)
DEFINE_LONG_GETTER(FrameNum, frame_num)
DEFINE_LONG_SETTER(FrameNum, frame_num)

static HRESULT WINAPI ShockwaveFlash_SetZoomRect(IShockwaveFlash *iface, LONG left, LONG top,
                                                  LONG right, LONG bottom)
{
    FIXME("(%p)->(%ld, %ld, %ld, %ld): no renderer\n", iface, left, top, right, bottom);
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Zoom(IShockwaveFlash *iface, int factor)
{
    FIXME("(%p)->(%d): no renderer\n", iface, factor);
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Pan(IShockwaveFlash *iface, LONG x, LONG y, int mode)
{
    FIXME("(%p)->(%ld, %ld, %d): no renderer\n", iface, x, y, mode);
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Play(IShockwaveFlash *iface)
{
    struct swflash *This = impl_from_IShockwaveFlash(iface);
    This->playing = VARIANT_TRUE;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Stop(IShockwaveFlash *iface)
{
    struct swflash *This = impl_from_IShockwaveFlash(iface);
    This->playing = VARIANT_FALSE;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Back(IShockwaveFlash *iface)
{
    struct swflash *This = impl_from_IShockwaveFlash(iface);
    if (This->frame_num > 0) --This->frame_num;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Forward(IShockwaveFlash *iface)
{
    ++impl_from_IShockwaveFlash(iface)->frame_num;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_Rewind(IShockwaveFlash *iface)
{
    impl_from_IShockwaveFlash(iface)->frame_num = 0;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_StopPlay(IShockwaveFlash *iface)
{
    return ShockwaveFlash_Stop(iface);
}

static HRESULT WINAPI ShockwaveFlash_GotoFrame(IShockwaveFlash *iface, LONG frame)
{
    impl_from_IShockwaveFlash(iface)->frame_num = frame;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_CurrentFrame(IShockwaveFlash *iface, LONG *frame)
{
    if (!frame) return E_POINTER;
    *frame = impl_from_IShockwaveFlash(iface)->frame_num;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_IsPlaying(IShockwaveFlash *iface, VARIANT_BOOL *playing)
{
    return ShockwaveFlash_get_Playing(iface, playing);
}

static HRESULT WINAPI ShockwaveFlash_PercentLoaded(IShockwaveFlash *iface, LONG *percent)
{
    if (!percent) return E_POINTER;
    *percent = 0;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_FrameLoaded(IShockwaveFlash *iface, LONG frame,
                                                  VARIANT_BOOL *loaded)
{
    if (!loaded) return E_POINTER;
    *loaded = VARIANT_FALSE;
    return S_OK;
}

static HRESULT WINAPI ShockwaveFlash_FlashVersion(IShockwaveFlash *iface, LONG *version)
{
    if (!version) return E_POINTER;
    *version = SWFLASH_VERSION;
    return S_OK;
}

DEFINE_BSTR_PROPERTY(WMode, wmode)
DEFINE_BSTR_PROPERTY(SAlign, salign)
DEFINE_BOOL_GETTER(Menu, menu)
DEFINE_BOOL_SETTER(Menu, menu)
DEFINE_BSTR_PROPERTY(Base, base)
DEFINE_BSTR_PROPERTY(Scale, scale)
DEFINE_BOOL_GETTER(DeviceFont, device_font)
DEFINE_BOOL_SETTER(DeviceFont, device_font)
DEFINE_BOOL_GETTER(EmbedMovie, embed_movie)
DEFINE_BOOL_SETTER(EmbedMovie, embed_movie)
DEFINE_BSTR_PROPERTY(BGColor, bgcolor)
DEFINE_BSTR_PROPERTY(Quality2, quality2)

static HRESULT WINAPI ShockwaveFlash_LoadMovie(IShockwaveFlash *iface, int layer, BSTR url)
{
    struct swflash *This = impl_from_IShockwaveFlash(iface);

    if (layer) return E_NOTIMPL;
    return set_bstr(This, &This->movie, url);
}

static HRESULT WINAPI ShockwaveFlash_TGotoFrame(IShockwaveFlash *iface, BSTR target, LONG frame)
{
    if (target && *target) return E_NOTIMPL;
    return ShockwaveFlash_GotoFrame(iface, frame);
}

static HRESULT WINAPI ShockwaveFlash_TGotoLabel(IShockwaveFlash *iface, BSTR target, BSTR label)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI ShockwaveFlash_TCurrentFrame(IShockwaveFlash *iface, BSTR target, LONG *frame)
{
    if (target && *target) return E_NOTIMPL;
    return ShockwaveFlash_CurrentFrame(iface, frame);
}

static HRESULT WINAPI ShockwaveFlash_TCurrentLabel(IShockwaveFlash *iface, BSTR target, BSTR *label)
{
    return get_bstr(NULL, label);
}

static HRESULT WINAPI ShockwaveFlash_TPlay(IShockwaveFlash *iface, BSTR target)
{
    if (target && *target) return E_NOTIMPL;
    return ShockwaveFlash_Play(iface);
}

static HRESULT WINAPI ShockwaveFlash_TStopPlay(IShockwaveFlash *iface, BSTR target)
{
    if (target && *target) return E_NOTIMPL;
    return ShockwaveFlash_Stop(iface);
}

static HRESULT WINAPI ShockwaveFlash_SetVariable(IShockwaveFlash *iface, BSTR name, BSTR value)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI ShockwaveFlash_GetVariable(IShockwaveFlash *iface, BSTR name, BSTR *value)
{
    return get_bstr(NULL, value);
}

static HRESULT WINAPI ShockwaveFlash_TSetProperty(IShockwaveFlash *iface, BSTR target,
                                                   int property, BSTR value)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI ShockwaveFlash_TGetProperty(IShockwaveFlash *iface, BSTR target,
                                                   int property, BSTR *value)
{
    return get_bstr(NULL, value);
}

static HRESULT WINAPI ShockwaveFlash_TCallFrame(IShockwaveFlash *iface, BSTR target, int frame)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI ShockwaveFlash_TCallLabel(IShockwaveFlash *iface, BSTR target, BSTR label)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI ShockwaveFlash_TSetPropertyNum(IShockwaveFlash *iface, BSTR target,
                                                      int property, double value)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI ShockwaveFlash_TGetPropertyNum(IShockwaveFlash *iface, BSTR target,
                                                      int property, double *value)
{
    if (!value) return E_POINTER;
    *value = 0.0;
    return E_NOTIMPL;
}

DEFINE_BSTR_PROPERTY(SWRemote, swremote)

static const IShockwaveFlashVtbl ShockwaveFlashVtbl =
{
    ShockwaveFlash_QueryInterface,
    ShockwaveFlash_AddRef,
    ShockwaveFlash_Release,
    ShockwaveFlash_GetTypeInfoCount,
    ShockwaveFlash_GetTypeInfo,
    ShockwaveFlash_GetIDsOfNames,
    ShockwaveFlash_Invoke,
    ShockwaveFlash_get_ReadyState,
    ShockwaveFlash_get_TotalFrames,
    ShockwaveFlash_get_Playing,
    ShockwaveFlash_put_Playing,
    ShockwaveFlash_get_Quality,
    ShockwaveFlash_put_Quality,
    ShockwaveFlash_get_ScaleMode,
    ShockwaveFlash_put_ScaleMode,
    ShockwaveFlash_get_AlignMode,
    ShockwaveFlash_put_AlignMode,
    ShockwaveFlash_get_BackgroundColor,
    ShockwaveFlash_put_BackgroundColor,
    ShockwaveFlash_get_Loop,
    ShockwaveFlash_put_Loop,
    ShockwaveFlash_get_Movie,
    ShockwaveFlash_put_Movie,
    ShockwaveFlash_get_FrameNum,
    ShockwaveFlash_put_FrameNum,
    ShockwaveFlash_SetZoomRect,
    ShockwaveFlash_Zoom,
    ShockwaveFlash_Pan,
    ShockwaveFlash_Play,
    ShockwaveFlash_Stop,
    ShockwaveFlash_Back,
    ShockwaveFlash_Forward,
    ShockwaveFlash_Rewind,
    ShockwaveFlash_StopPlay,
    ShockwaveFlash_GotoFrame,
    ShockwaveFlash_CurrentFrame,
    ShockwaveFlash_IsPlaying,
    ShockwaveFlash_PercentLoaded,
    ShockwaveFlash_FrameLoaded,
    ShockwaveFlash_FlashVersion,
    ShockwaveFlash_get_WMode,
    ShockwaveFlash_put_WMode,
    ShockwaveFlash_get_SAlign,
    ShockwaveFlash_put_SAlign,
    ShockwaveFlash_get_Menu,
    ShockwaveFlash_put_Menu,
    ShockwaveFlash_get_Base,
    ShockwaveFlash_put_Base,
    ShockwaveFlash_get_Scale,
    ShockwaveFlash_put_Scale,
    ShockwaveFlash_get_DeviceFont,
    ShockwaveFlash_put_DeviceFont,
    ShockwaveFlash_get_EmbedMovie,
    ShockwaveFlash_put_EmbedMovie,
    ShockwaveFlash_get_BGColor,
    ShockwaveFlash_put_BGColor,
    ShockwaveFlash_get_Quality2,
    ShockwaveFlash_put_Quality2,
    ShockwaveFlash_LoadMovie,
    ShockwaveFlash_TGotoFrame,
    ShockwaveFlash_TGotoLabel,
    ShockwaveFlash_TCurrentFrame,
    ShockwaveFlash_TCurrentLabel,
    ShockwaveFlash_TPlay,
    ShockwaveFlash_TStopPlay,
    ShockwaveFlash_SetVariable,
    ShockwaveFlash_GetVariable,
    ShockwaveFlash_TSetProperty,
    ShockwaveFlash_TGetProperty,
    ShockwaveFlash_TCallFrame,
    ShockwaveFlash_TCallLabel,
    ShockwaveFlash_TSetPropertyNum,
    ShockwaveFlash_TGetPropertyNum,
    ShockwaveFlash_get_SWRemote,
    ShockwaveFlash_put_SWRemote
};

/* IOleObject */

static HRESULT WINAPI OleObject_QueryInterface(IOleObject *iface, REFIID iid, void **out)
{
    return swflash_query_interface(impl_from_IOleObject(iface), iid, out);
}

static ULONG WINAPI OleObject_AddRef(IOleObject *iface)
{
    return swflash_addref(impl_from_IOleObject(iface));
}

static ULONG WINAPI OleObject_Release(IOleObject *iface)
{
    return swflash_release(impl_from_IOleObject(iface));
}

static HRESULT WINAPI OleObject_SetClientSite(IOleObject *iface, IOleClientSite *site)
{
    struct swflash *This = impl_from_IOleObject(iface);

    if (site) IOleClientSite_AddRef(site);
    if (This->client_site) IOleClientSite_Release(This->client_site);
    This->client_site = site;
    return S_OK;
}

static HRESULT WINAPI OleObject_GetClientSite(IOleObject *iface, IOleClientSite **site)
{
    struct swflash *This = impl_from_IOleObject(iface);

    if (!site) return E_POINTER;
    *site = This->client_site;
    if (*site) IOleClientSite_AddRef(*site);
    return S_OK;
}

static HRESULT WINAPI OleObject_SetHostNames(IOleObject *iface, LPCOLESTR app, LPCOLESTR object)
{
    return S_OK;
}

static HRESULT WINAPI OleObject_Close(IOleObject *iface, DWORD save)
{
    return S_OK;
}

static HRESULT WINAPI OleObject_SetMoniker(IOleObject *iface, DWORD which, IMoniker *moniker)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetMoniker(IOleObject *iface, DWORD assign, DWORD which,
                                            IMoniker **moniker)
{
    if (!moniker) return E_POINTER;
    *moniker = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_InitFromData(IOleObject *iface, IDataObject *data, BOOL creation,
                                              DWORD reserved)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetClipboardData(IOleObject *iface, DWORD reserved,
                                                  IDataObject **data)
{
    if (!data) return E_POINTER;
    *data = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_DoVerb(IOleObject *iface, LONG verb, LPMSG msg,
                                        IOleClientSite *active_site, LONG index,
                                        HWND parent, LPCRECT rect)
{
    switch (verb)
    {
    case OLEIVERB_HIDE:
        return S_OK;
    case OLEIVERB_PRIMARY:
    case OLEIVERB_SHOW:
    case OLEIVERB_INPLACEACTIVATE:
    case OLEIVERB_UIACTIVATE:
        FIXME("verb %ld requested without a Flash renderer\n", verb);
        return E_NOTIMPL;
    default:
        return OLEOBJ_S_INVALIDVERB;
    }
}

static HRESULT WINAPI OleObject_EnumVerbs(IOleObject *iface, IEnumOLEVERB **verbs)
{
    if (!verbs) return E_POINTER;
    *verbs = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_Update(IOleObject *iface)
{
    return S_OK;
}

static HRESULT WINAPI OleObject_IsUpToDate(IOleObject *iface)
{
    return S_OK;
}

static HRESULT WINAPI OleObject_GetUserClassID(IOleObject *iface, CLSID *clsid)
{
    if (!clsid) return E_POINTER;
    *clsid = CLSID_ShockwaveFlash;
    return S_OK;
}

static HRESULT WINAPI OleObject_GetUserType(IOleObject *iface, DWORD form, LPOLESTR *type)
{
    static const WCHAR name[] = L"Shockwave Flash Object";

    if (!type) return E_POINTER;
    if (!(*type = CoTaskMemAlloc(sizeof(name)))) return E_OUTOFMEMORY;
    memcpy(*type, name, sizeof(name));
    return S_OK;
}

static HRESULT WINAPI OleObject_SetExtent(IOleObject *iface, DWORD aspect, SIZEL *size)
{
    struct swflash *This = impl_from_IOleObject(iface);

    if (!size) return E_POINTER;
    if (aspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    This->extent = *size;
    This->dirty = TRUE;
    return S_OK;
}

static HRESULT WINAPI OleObject_GetExtent(IOleObject *iface, DWORD aspect, SIZEL *size)
{
    struct swflash *This = impl_from_IOleObject(iface);

    if (!size) return E_POINTER;
    if (aspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    *size = This->extent;
    return S_OK;
}

static HRESULT WINAPI OleObject_Advise(IOleObject *iface, IAdviseSink *sink, DWORD *connection)
{
    if (connection) *connection = 0;
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_Unadvise(IOleObject *iface, DWORD connection)
{
    return OLE_E_NOCONNECTION;
}

static HRESULT WINAPI OleObject_EnumAdvise(IOleObject *iface, IEnumSTATDATA **advise)
{
    if (!advise) return E_POINTER;
    *advise = NULL;
    return E_NOTIMPL;
}

static HRESULT WINAPI OleObject_GetMiscStatus(IOleObject *iface, DWORD aspect, DWORD *status)
{
    if (!status) return E_POINTER;
    if (aspect != DVASPECT_CONTENT) return DV_E_DVASPECT;
    *status = SWFLASH_MISC_STATUS;
    return S_OK;
}

static HRESULT WINAPI OleObject_SetColorScheme(IOleObject *iface, LOGPALETTE *palette)
{
    return S_OK;
}

static const IOleObjectVtbl OleObjectVtbl =
{
    OleObject_QueryInterface,
    OleObject_AddRef,
    OleObject_Release,
    OleObject_SetClientSite,
    OleObject_GetClientSite,
    OleObject_SetHostNames,
    OleObject_Close,
    OleObject_SetMoniker,
    OleObject_GetMoniker,
    OleObject_InitFromData,
    OleObject_GetClipboardData,
    OleObject_DoVerb,
    OleObject_EnumVerbs,
    OleObject_Update,
    OleObject_IsUpToDate,
    OleObject_GetUserClassID,
    OleObject_GetUserType,
    OleObject_SetExtent,
    OleObject_GetExtent,
    OleObject_Advise,
    OleObject_Unadvise,
    OleObject_EnumAdvise,
    OleObject_GetMiscStatus,
    OleObject_SetColorScheme
};

/* IPersistStreamInit */

static HRESULT WINAPI PersistStreamInit_QueryInterface(IPersistStreamInit *iface, REFIID iid,
                                                        void **out)
{
    return swflash_query_interface(impl_from_IPersistStreamInit(iface), iid, out);
}

static ULONG WINAPI PersistStreamInit_AddRef(IPersistStreamInit *iface)
{
    return swflash_addref(impl_from_IPersistStreamInit(iface));
}

static ULONG WINAPI PersistStreamInit_Release(IPersistStreamInit *iface)
{
    return swflash_release(impl_from_IPersistStreamInit(iface));
}

static HRESULT WINAPI PersistStreamInit_GetClassID(IPersistStreamInit *iface, CLSID *clsid)
{
    if (!clsid) return E_POINTER;
    *clsid = CLSID_ShockwaveFlash;
    return S_OK;
}

static HRESULT WINAPI PersistStreamInit_IsDirty(IPersistStreamInit *iface)
{
    return impl_from_IPersistStreamInit(iface)->dirty ? S_OK : S_FALSE;
}

static HRESULT WINAPI PersistStreamInit_Load(IPersistStreamInit *iface, IStream *stream)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_Save(IPersistStreamInit *iface, IStream *stream,
                                              BOOL clear_dirty)
{
    if (clear_dirty) impl_from_IPersistStreamInit(iface)->dirty = FALSE;
    return E_NOTIMPL;
}

static HRESULT WINAPI PersistStreamInit_GetSizeMax(IPersistStreamInit *iface, ULARGE_INTEGER *size)
{
    if (!size) return E_POINTER;
    size->QuadPart = 0;
    return S_OK;
}

static HRESULT WINAPI PersistStreamInit_InitNew(IPersistStreamInit *iface)
{
    impl_from_IPersistStreamInit(iface)->dirty = FALSE;
    return S_OK;
}

static const IPersistStreamInitVtbl PersistStreamInitVtbl =
{
    PersistStreamInit_QueryInterface,
    PersistStreamInit_AddRef,
    PersistStreamInit_Release,
    PersistStreamInit_GetClassID,
    PersistStreamInit_IsDirty,
    PersistStreamInit_Load,
    PersistStreamInit_Save,
    PersistStreamInit_GetSizeMax,
    PersistStreamInit_InitNew
};

/* IOleControl */

static HRESULT WINAPI OleControl_QueryInterface(IOleControl *iface, REFIID iid, void **out)
{
    return swflash_query_interface(impl_from_IOleControl(iface), iid, out);
}

static ULONG WINAPI OleControl_AddRef(IOleControl *iface)
{
    return swflash_addref(impl_from_IOleControl(iface));
}

static ULONG WINAPI OleControl_Release(IOleControl *iface)
{
    return swflash_release(impl_from_IOleControl(iface));
}

static HRESULT WINAPI OleControl_GetControlInfo(IOleControl *iface, CONTROLINFO *info)
{
    if (!info) return E_POINTER;
    info->cb = sizeof(*info);
    info->hAccel = NULL;
    info->cAccel = 0;
    info->dwFlags = 0;
    return S_OK;
}

static HRESULT WINAPI OleControl_OnMnemonic(IOleControl *iface, MSG *msg)
{
    return S_OK;
}

static HRESULT WINAPI OleControl_OnAmbientPropertyChange(IOleControl *iface, DISPID dispid)
{
    return S_OK;
}

static HRESULT WINAPI OleControl_FreezeEvents(IOleControl *iface, BOOL freeze)
{
    return S_OK;
}

static const IOleControlVtbl OleControlVtbl =
{
    OleControl_QueryInterface,
    OleControl_AddRef,
    OleControl_Release,
    OleControl_GetControlInfo,
    OleControl_OnMnemonic,
    OleControl_OnAmbientPropertyChange,
    OleControl_FreezeEvents
};

static HRESULT swflash_create(IUnknown *outer, REFIID iid, void **out)
{
    struct swflash *object;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;

    if (!(object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IShockwaveFlash_iface.lpVtbl = &ShockwaveFlashVtbl;
    object->IOleObject_iface.lpVtbl = &OleObjectVtbl;
    object->IPersistStreamInit_iface.lpVtbl = &PersistStreamInitVtbl;
    object->IOleControl_iface.lpVtbl = &OleControlVtbl;
    object->ref = 1;
    object->extent.cx = 8467;
    object->extent.cy = 6350;
    object->quality = 1;
    object->loop = VARIANT_TRUE;
    object->menu = VARIANT_TRUE;
    InterlockedIncrement(&object_count);

    hr = swflash_query_interface(object, iid, out);
    swflash_release(object);
    return hr;
}

/* FlashProp property page */

static HRESULT WINAPI FlashProp_QueryInterface(IPropertyPage *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IPropertyPage))
    {
        *out = iface;
        IPropertyPage_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI FlashProp_AddRef(IPropertyPage *iface)
{
    return InterlockedIncrement(&impl_from_IPropertyPage(iface)->ref);
}

static ULONG WINAPI FlashProp_Release(IPropertyPage *iface)
{
    struct flashprop *This = impl_from_IPropertyPage(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    if (!ref)
    {
        if (This->site) IPropertyPageSite_Release(This->site);
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&object_count);
    }
    return ref;
}

static HRESULT WINAPI FlashProp_SetPageSite(IPropertyPage *iface, IPropertyPageSite *site)
{
    struct flashprop *This = impl_from_IPropertyPage(iface);

    if (site) IPropertyPageSite_AddRef(site);
    if (This->site) IPropertyPageSite_Release(This->site);
    This->site = site;
    return S_OK;
}

static HRESULT WINAPI FlashProp_Activate(IPropertyPage *iface, HWND parent, LPCRECT rect, BOOL modal)
{
    FIXME("(%p, %p, %p, %d): property-page UI is not implemented\n", iface, parent, rect, modal);
    return E_NOTIMPL;
}

static HRESULT WINAPI FlashProp_Deactivate(IPropertyPage *iface)
{
    return S_OK;
}

static HRESULT WINAPI FlashProp_GetPageInfo(IPropertyPage *iface, PROPPAGEINFO *info)
{
    static const WCHAR title[] = L"Shockwave Flash";

    if (!info) return E_POINTER;
    memset(info, 0, sizeof(*info));
    info->cb = sizeof(*info);
    info->size.cx = 250;
    info->size.cy = 150;
    if (!(info->pszTitle = CoTaskMemAlloc(sizeof(title)))) return E_OUTOFMEMORY;
    memcpy(info->pszTitle, title, sizeof(title));
    return S_OK;
}

static HRESULT WINAPI FlashProp_SetObjects(IPropertyPage *iface, ULONG count, IUnknown **objects)
{
    return S_OK;
}

static HRESULT WINAPI FlashProp_Show(IPropertyPage *iface, UINT command)
{
    return command == SW_HIDE ? S_OK : E_NOTIMPL;
}

static HRESULT WINAPI FlashProp_Move(IPropertyPage *iface, LPCRECT rect)
{
    return S_OK;
}

static HRESULT WINAPI FlashProp_IsPageDirty(IPropertyPage *iface)
{
    return S_FALSE;
}

static HRESULT WINAPI FlashProp_Apply(IPropertyPage *iface)
{
    return S_OK;
}

static HRESULT WINAPI FlashProp_Help(IPropertyPage *iface, LPCOLESTR help_dir)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI FlashProp_TranslateAccelerator(IPropertyPage *iface, MSG *msg)
{
    return S_FALSE;
}

static const IPropertyPageVtbl FlashPropVtbl =
{
    FlashProp_QueryInterface,
    FlashProp_AddRef,
    FlashProp_Release,
    FlashProp_SetPageSite,
    FlashProp_Activate,
    FlashProp_Deactivate,
    FlashProp_GetPageInfo,
    FlashProp_SetObjects,
    FlashProp_Show,
    FlashProp_Move,
    FlashProp_IsPageDirty,
    FlashProp_Apply,
    FlashProp_Help,
    FlashProp_TranslateAccelerator
};

static HRESULT flashprop_create(IUnknown *outer, REFIID iid, void **out)
{
    struct flashprop *object;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;

    if (!(object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IPropertyPage_iface.lpVtbl = &FlashPropVtbl;
    object->ref = 1;
    InterlockedIncrement(&object_count);

    hr = IPropertyPage_QueryInterface(&object->IPropertyPage_iface, iid, out);
    IPropertyPage_Release(&object->IPropertyPage_iface);
    return hr;
}

/* IClassFactory */

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (!IsEqualIID(iid, &IID_IUnknown) && !IsEqualIID(iid, &IID_IClassFactory))
        return E_NOINTERFACE;

    *out = iface;
    IClassFactory_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    return InterlockedIncrement(&factory_refs);
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    LONG refs;

    do
    {
        refs = factory_refs;
        if (!refs) return 0;
    } while (InterlockedCompareExchange(&factory_refs, refs - 1, refs) != refs);

    return refs - 1;
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *outer,
                                                   REFIID iid, void **out)
{
    return impl_from_IClassFactory(iface)->create(outer, iid, out);
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL lock)
{
    LONG current;

    if (lock)
        InterlockedIncrement(&server_locks);
    else
    {
        do
        {
            current = server_locks;
            if (!current) break;
        } while (InterlockedCompareExchange(&server_locks, current - 1, current) != current);
    }
    return S_OK;
}

static const IClassFactoryVtbl ClassFactoryVtbl =
{
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ClassFactory_CreateInstance,
    ClassFactory_LockServer
};

static struct class_factory shockwave_factory = { { &ClassFactoryVtbl }, swflash_create };
static struct class_factory flashprop_factory = { { &ClassFactoryVtbl }, flashprop_create };

BOOL WINAPI DllMain(HINSTANCE dll, DWORD reason, void *reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        instance = dll;
        DisableThreadLibraryCalls(dll);
    }
    else if (reason == DLL_PROCESS_DETACH && !reserved && shockwave_typeinfo)
    {
        ITypeInfo_Release(shockwave_typeinfo);
        shockwave_typeinfo = NULL;
    }
    return TRUE;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    TRACE("(%s, %s, %p)\n", debugstr_guid(clsid), debugstr_guid(iid), out);

    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualGUID(clsid, &CLSID_ShockwaveFlash))
        return IClassFactory_QueryInterface(&shockwave_factory.IClassFactory_iface, iid, out);
    if (IsEqualGUID(clsid, &CLSID_FlashProp))
        return IClassFactory_QueryInterface(&flashprop_factory.IClassFactory_iface, iid, out);

    return CLASS_E_CLASSNOTAVAILABLE;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return object_count || factory_refs || server_locks ? S_FALSE : S_OK;
}
