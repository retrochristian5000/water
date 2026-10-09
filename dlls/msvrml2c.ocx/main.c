/*
 * Microsoft VRML 2.0 viewer (msvrml2c.ocx): initial COM ABI.
 *
 * Copyright 2026 the Water contributors.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Microsoft licensed Intervista WorldView for the historical viewer.
 * This is independent Water code, not a VRML renderer or a reimplementation
 * of the original ActiveX automation interface.
 */

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "objbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msvrml2c);

/* VRML 2.0 Viewer component ID and observed VRMLBrowser COM class.
 * Do not reuse the distinct DirectAnimation viewer CLSIDs. */
static const GUID clsid_vrml_browser = {0x90a7533d, 0x88fe, 0x11d0,
    {0x9d, 0xbe, 0x00, 0x00, 0xc0, 0x41, 0x1f, 0xc3}};

struct vrml_factory
{
    IClassFactory IClassFactory_iface;
    LONG refs;
};

static LONG active_factories;
static LONG server_locks;

static inline struct vrml_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct vrml_factory, IClassFactory_iface);
}

static HRESULT WINAPI factory_QueryInterface(IClassFactory *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory))
    {
        *out = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef(IClassFactory *iface)
{
    return InterlockedIncrement(&impl_from_IClassFactory(iface)->refs);
}

static ULONG WINAPI factory_Release(IClassFactory *iface)
{
    struct vrml_factory *factory = impl_from_IClassFactory(iface);
    ULONG refs = InterlockedDecrement(&factory->refs);

    if (!refs)
    {
        HeapFree(GetProcessHeap(), 0, factory);
        InterlockedDecrement(&active_factories);
    }
    return refs;
}

static HRESULT WINAPI factory_CreateInstance(IClassFactory *iface, IUnknown *outer,
                                               REFIID iid, void **out)
{
    (void)iface;
    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;

    /* No VRML browser instance until native ActiveX/IDL contracts and
     * at least a functional scene renderer are implemented. */
    FIXME("VRML 2.0 ActiveX browser not implemented (%s).\n", debugstr_guid(iid));
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_LockServer(IClassFactory *iface, BOOL lock)
{
    LONG old_locks;
    (void)iface;

    if (lock)
    {
        InterlockedIncrement(&server_locks);
        return S_OK;
    }
    do
    {
        old_locks = InterlockedCompareExchange(&server_locks, 0, 0);
        if (!old_locks) return E_UNEXPECTED;
    } while (InterlockedCompareExchange(&server_locks, old_locks - 1, old_locks) != old_locks);
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

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    struct vrml_factory *factory;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(clsid, &clsid_vrml_browser))
        return CLASS_E_CLASSNOTAVAILABLE;

    if (!(factory = HeapAlloc(GetProcessHeap(), 0, sizeof(*factory))))
        return E_OUTOFMEMORY;
    factory->IClassFactory_iface.lpVtbl = &factory_vtbl;
    factory->refs = 1;
    InterlockedIncrement(&active_factories);

    hr = IClassFactory_QueryInterface(&factory->IClassFactory_iface, iid, out);
    IClassFactory_Release(&factory->IClassFactory_iface);
    return hr;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return !active_factories && !server_locks ? S_OK : S_FALSE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    /* Avoid overriding a working native Intervista/Microsoft VRML viewer.
     * This module does not yet implement IOleObject, in-place activation,
     * IDispatch, scene rendering, or safe file/URL loading. */
    FIXME("VRML ActiveX registration is disabled until a viewer exists.\n");
    return SELFREG_E_CLASS;
}

HRESULT WINAPI DllUnregisterServer(void)
{
    /* No registry keys have been created by this implementation. */
    return S_OK;
}
