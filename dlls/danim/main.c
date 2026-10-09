/*
 * DirectAnimation (DirectX Media) COM entry points.
 *
 * The viewer class factories are a first-stage implementation only.
 * Their objects, automation interfaces, and rendering are not available.
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
#include "objbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(danim);

/* Both were published for the early DirectAnimation ActiveX viewers.
 * Their dispatch interfaces are distinct and are not yet implemented. */
static const GUID clsid_viewer = {0xb6ffc24c, 0x7e13, 0x11d0,
    {0x9b, 0x47, 0x00, 0xc0, 0x4f, 0xc2, 0xf5, 0x1d}};
static const GUID clsid_control = {0x69ad90ef, 0x1c20, 0x11d1,
    {0x88, 0x01, 0x00, 0xc0, 0x4f, 0xc2, 0x9d, 0x46}};

struct danim_factory
{
    IClassFactory IClassFactory_iface;
    LONG refs;
};

static LONG live_factories;
static LONG server_locks;

static inline struct danim_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct danim_factory, IClassFactory_iface);
}

static HRESULT WINAPI factory_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    if (!out)
        return E_POINTER;
    *out = NULL;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory))
    {
        *out = iface;
        IClassFactory_AddRef(iface);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG WINAPI factory_AddRef(IClassFactory *iface)
{
    struct danim_factory *factory = impl_from_IClassFactory(iface);
    return InterlockedIncrement(&factory->refs);
}

static ULONG WINAPI factory_Release(IClassFactory *iface)
{
    struct danim_factory *factory = impl_from_IClassFactory(iface);
    ULONG refs = InterlockedDecrement(&factory->refs);

    if (!refs)
    {
        HeapFree(GetProcessHeap(), 0, factory);
        InterlockedDecrement(&live_factories);
    }
    return refs;
}

static HRESULT WINAPI factory_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    if (!out)
        return E_POINTER;
    *out = NULL;

    if (outer)
        return CLASS_E_NOAGGREGATION;

    /* Do not fabricate an IDispatch/IOleObject without its original
     * type-library and ActiveX control semantics. */
    FIXME("DirectAnimation viewer creation is not implemented (%s).\n", debugstr_guid(riid));
    return E_NOTIMPL;
}

static HRESULT WINAPI factory_LockServer(IClassFactory *iface, BOOL lock)
{
    LONG locks;

    if (lock)
    {
        InterlockedIncrement(&server_locks);
        return S_OK;
    }

    do
    {
        locks = InterlockedCompareExchange(&server_locks, 0, 0);
        if (!locks)
            return E_UNEXPECTED;
    } while (InterlockedCompareExchange(&server_locks, locks - 1, locks) != locks);
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

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID riid, void **out)
{
    struct danim_factory *factory;
    HRESULT hr;

    if (!out)
        return E_POINTER;
    *out = NULL;

    if (!IsEqualGUID(clsid, &clsid_viewer) && !IsEqualGUID(clsid, &clsid_control))
        return CLASS_E_CLASSNOTAVAILABLE;

    if (!(factory = HeapAlloc(GetProcessHeap(), 0, sizeof(*factory))))
        return E_OUTOFMEMORY;
    factory->IClassFactory_iface.lpVtbl = &factory_vtbl;
    factory->refs = 1;
    InterlockedIncrement(&live_factories);

    hr = IClassFactory_QueryInterface(&factory->IClassFactory_iface, riid, out);
    IClassFactory_Release(&factory->IClassFactory_iface);
    return hr;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return !live_factories && !server_locks ? S_OK : S_FALSE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    /* Claiming the original ActiveX CLSIDs before the viewer can render
     * would make old IE pages activate a nonfunctional control. */
    FIXME("DirectAnimation ActiveX registration requires a working viewer.\n");
    return SELFREG_E_CLASS;
}

HRESULT WINAPI DllUnregisterServer(void)
{
    /* This implementation has never registered either viewer CLSID.
     * In particular, do not delete an existing native Windows binding. */
    return S_OK;
}
