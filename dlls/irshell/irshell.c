/*
 * Infrared Recipient shell extension
 *
 * Copyright 2026 Vincent and the Wine project
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
 */

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "objbase.h"
#include "shlobj.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(irshell);

/*
 * Windows registers this class as the "Infrared Recipient" shell namespace
 * object and context-menu handler.
 */
static const CLSID CLSID_InfraredRecipient =
    {0x00435ae0,0xbffb,0x11cf,{0xa9,0xd8,0x00,0xaa,0x00,0x42,0x35,0x96}};

static LONG object_count;
static LONG server_locks;

struct irshell
{
    IShellFolder IShellFolder_iface;
    IPersistFolder IPersistFolder_iface;
    IContextMenu IContextMenu_iface;
    IShellExtInit IShellExtInit_iface;
    LONG ref;
    LPITEMIDLIST pidl;
};

struct empty_enum
{
    IEnumIDList IEnumIDList_iface;
    LONG ref;
};

struct class_factory
{
    IClassFactory IClassFactory_iface;
    LONG ref;
};

static inline struct irshell *impl_from_IShellFolder(IShellFolder *iface)
{
    return CONTAINING_RECORD(iface, struct irshell, IShellFolder_iface);
}

static inline struct irshell *impl_from_IPersistFolder(IPersistFolder *iface)
{
    return CONTAINING_RECORD(iface, struct irshell, IPersistFolder_iface);
}

static inline struct irshell *impl_from_IContextMenu(IContextMenu *iface)
{
    return CONTAINING_RECORD(iface, struct irshell, IContextMenu_iface);
}

static inline struct irshell *impl_from_IShellExtInit(IShellExtInit *iface)
{
    return CONTAINING_RECORD(iface, struct irshell, IShellExtInit_iface);
}

static inline struct empty_enum *impl_from_IEnumIDList(IEnumIDList *iface)
{
    return CONTAINING_RECORD(iface, struct empty_enum, IEnumIDList_iface);
}

static inline struct class_factory *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, struct class_factory, IClassFactory_iface);
}

static HRESULT irshell_query_interface(struct irshell *This, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IShellFolder))
        *out = &This->IShellFolder_iface;
    else if (IsEqualIID(iid, &IID_IPersist) || IsEqualIID(iid, &IID_IPersistFolder))
        *out = &This->IPersistFolder_iface;
    else if (IsEqualIID(iid, &IID_IContextMenu))
        *out = &This->IContextMenu_iface;
    else if (IsEqualIID(iid, &IID_IShellExtInit))
        *out = &This->IShellExtInit_iface;
    else
        return E_NOINTERFACE;

    InterlockedIncrement(&This->ref);
    return S_OK;
}

static ULONG irshell_addref(struct irshell *This)
{
    return InterlockedIncrement(&This->ref);
}

static ULONG irshell_release(struct irshell *This)
{
    ULONG ref = InterlockedDecrement(&This->ref);

    if (!ref)
    {
        ILFree(This->pidl);
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&object_count);
    }
    return ref;
}

/* IEnumIDList: the transport-less compatibility folder has no children. */

static HRESULT WINAPI empty_enum_QueryInterface(IEnumIDList *iface, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IEnumIDList))
    {
        *out = iface;
        IEnumIDList_AddRef(iface);
        return S_OK;
    }
    return E_NOINTERFACE;
}

static ULONG WINAPI empty_enum_AddRef(IEnumIDList *iface)
{
    struct empty_enum *This = impl_from_IEnumIDList(iface);
    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI empty_enum_Release(IEnumIDList *iface)
{
    struct empty_enum *This = impl_from_IEnumIDList(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    if (!ref)
    {
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&object_count);
    }
    return ref;
}

static HRESULT WINAPI empty_enum_Next(IEnumIDList *iface, ULONG count, LPITEMIDLIST *items,
                                      ULONG *fetched)
{
    ULONG i;

    if (!items || (count != 1 && !fetched)) return E_POINTER;
    for (i = 0; i < count; ++i) items[i] = NULL;
    if (fetched) *fetched = 0;
    return S_FALSE;
}

static HRESULT WINAPI empty_enum_Skip(IEnumIDList *iface, ULONG count)
{
    return S_FALSE;
}

static HRESULT WINAPI empty_enum_Reset(IEnumIDList *iface)
{
    return S_OK;
}

static HRESULT empty_enum_create(IEnumIDList **out);

static HRESULT WINAPI empty_enum_Clone(IEnumIDList *iface, IEnumIDList **out)
{
    return empty_enum_create(out);
}

static const IEnumIDListVtbl empty_enum_vtbl =
{
    empty_enum_QueryInterface,
    empty_enum_AddRef,
    empty_enum_Release,
    empty_enum_Next,
    empty_enum_Skip,
    empty_enum_Reset,
    empty_enum_Clone
};

static HRESULT empty_enum_create(IEnumIDList **out)
{
    struct empty_enum *enumerator;

    if (!out) return E_POINTER;
    *out = NULL;

    if (!(enumerator = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*enumerator))))
        return E_OUTOFMEMORY;

    enumerator->IEnumIDList_iface.lpVtbl = &empty_enum_vtbl;
    enumerator->ref = 1;
    InterlockedIncrement(&object_count);
    *out = &enumerator->IEnumIDList_iface;
    return S_OK;
}

/* IShellFolder */

static HRESULT WINAPI shellfolder_QueryInterface(IShellFolder *iface, REFIID iid, void **out)
{
    return irshell_query_interface(impl_from_IShellFolder(iface), iid, out);
}

static ULONG WINAPI shellfolder_AddRef(IShellFolder *iface)
{
    return irshell_addref(impl_from_IShellFolder(iface));
}

static ULONG WINAPI shellfolder_Release(IShellFolder *iface)
{
    return irshell_release(impl_from_IShellFolder(iface));
}

static HRESULT WINAPI shellfolder_ParseDisplayName(IShellFolder *iface, HWND hwnd, IBindCtx *bind_ctx,
                                                    LPOLESTR name, ULONG *eaten, LPITEMIDLIST *pidl,
                                                    ULONG *attributes)
{
    TRACE("(%p, %p, %p, %s, %p, %p, %p)\n", iface, hwnd, bind_ctx,
          debugstr_w(name), eaten, pidl, attributes);

    if (!pidl) return E_POINTER;
    *pidl = NULL;
    if (eaten) *eaten = 0;
    return E_NOTIMPL;
}

static HRESULT WINAPI shellfolder_EnumObjects(IShellFolder *iface, HWND hwnd, SHCONTF flags,
                                               IEnumIDList **out)
{
    TRACE("(%p, %p, %#lx, %p)\n", iface, hwnd, flags, out);
    return empty_enum_create(out);
}

static HRESULT WINAPI shellfolder_BindToObject(IShellFolder *iface, LPCITEMIDLIST pidl,
                                                IBindCtx *bind_ctx, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOINTERFACE;
}

static HRESULT WINAPI shellfolder_BindToStorage(IShellFolder *iface, LPCITEMIDLIST pidl,
                                                 IBindCtx *bind_ctx, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOINTERFACE;
}

static HRESULT WINAPI shellfolder_CompareIDs(IShellFolder *iface, LPARAM param,
                                              LPCITEMIDLIST pidl1, LPCITEMIDLIST pidl2)
{
    if (pidl1 == pidl2) return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
    return E_INVALIDARG;
}

static HRESULT WINAPI shellfolder_CreateViewObject(IShellFolder *iface, HWND hwnd,
                                                    REFIID iid, void **out)
{
    CSFV view = { sizeof(view), iface, NULL, NULL, 0, NULL, FVM_AUTO };

    if (!out) return E_POINTER;
    *out = NULL;

    if (IsEqualIID(iid, &IID_IShellView))
        return SHCreateShellFolderViewEx(&view, (IShellView **)out);
    if (IsEqualIID(iid, &IID_IContextMenu))
        return IShellFolder_QueryInterface(iface, iid, out);

    return E_NOINTERFACE;
}

static HRESULT WINAPI shellfolder_GetAttributesOf(IShellFolder *iface, UINT count,
                                                   LPCITEMIDLIST *pidls, SFGAOF *attributes)
{
    static const SFGAOF supported = SFGAO_FILESYSTEM | SFGAO_DROPTARGET;

    if (!attributes) return E_POINTER;
    if (count) return E_INVALIDARG;

    *attributes &= supported;
    return S_OK;
}

static HRESULT WINAPI shellfolder_GetUIObjectOf(IShellFolder *iface, HWND hwnd, UINT count,
                                                 LPCITEMIDLIST *pidls, REFIID iid,
                                                 UINT *reserved, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    return E_NOINTERFACE;
}

static HRESULT WINAPI shellfolder_GetDisplayNameOf(IShellFolder *iface, LPCITEMIDLIST pidl,
                                                    SHGDNF flags, STRRET *name)
{
    if (!name) return E_POINTER;
    return E_INVALIDARG;
}

static HRESULT WINAPI shellfolder_SetNameOf(IShellFolder *iface, HWND hwnd, LPCITEMIDLIST pidl,
                                             LPCOLESTR name, SHGDNF flags, LPITEMIDLIST *out)
{
    if (out) *out = NULL;
    return E_NOTIMPL;
}

static const IShellFolderVtbl shellfolder_vtbl =
{
    shellfolder_QueryInterface,
    shellfolder_AddRef,
    shellfolder_Release,
    shellfolder_ParseDisplayName,
    shellfolder_EnumObjects,
    shellfolder_BindToObject,
    shellfolder_BindToStorage,
    shellfolder_CompareIDs,
    shellfolder_CreateViewObject,
    shellfolder_GetAttributesOf,
    shellfolder_GetUIObjectOf,
    shellfolder_GetDisplayNameOf,
    shellfolder_SetNameOf
};

/* IPersistFolder */

static HRESULT WINAPI persistfolder_QueryInterface(IPersistFolder *iface, REFIID iid, void **out)
{
    return irshell_query_interface(impl_from_IPersistFolder(iface), iid, out);
}

static ULONG WINAPI persistfolder_AddRef(IPersistFolder *iface)
{
    return irshell_addref(impl_from_IPersistFolder(iface));
}

static ULONG WINAPI persistfolder_Release(IPersistFolder *iface)
{
    return irshell_release(impl_from_IPersistFolder(iface));
}

static HRESULT WINAPI persistfolder_GetClassID(IPersistFolder *iface, CLSID *clsid)
{
    if (!clsid) return E_POINTER;
    *clsid = CLSID_InfraredRecipient;
    return S_OK;
}

static HRESULT WINAPI persistfolder_Initialize(IPersistFolder *iface, LPCITEMIDLIST pidl)
{
    struct irshell *This = impl_from_IPersistFolder(iface);
    LPITEMIDLIST clone = NULL;

    if (pidl && !(clone = ILClone(pidl))) return E_OUTOFMEMORY;
    ILFree(This->pidl);
    This->pidl = clone;
    return S_OK;
}

static const IPersistFolderVtbl persistfolder_vtbl =
{
    persistfolder_QueryInterface,
    persistfolder_AddRef,
    persistfolder_Release,
    persistfolder_GetClassID,
    persistfolder_Initialize
};

/* IContextMenu */

static HRESULT WINAPI contextmenu_QueryInterface(IContextMenu *iface, REFIID iid, void **out)
{
    return irshell_query_interface(impl_from_IContextMenu(iface), iid, out);
}

static ULONG WINAPI contextmenu_AddRef(IContextMenu *iface)
{
    return irshell_addref(impl_from_IContextMenu(iface));
}

static ULONG WINAPI contextmenu_Release(IContextMenu *iface)
{
    return irshell_release(impl_from_IContextMenu(iface));
}

static HRESULT WINAPI contextmenu_QueryContextMenu(IContextMenu *iface, HMENU menu, UINT index,
                                                    UINT first, UINT last, UINT flags)
{
    TRACE("(%p, %p, %u, %u, %u, %#x)\n", iface, menu, index, first, last, flags);
    return MAKE_HRESULT(SEVERITY_SUCCESS, 0, 0);
}

static HRESULT WINAPI contextmenu_InvokeCommand(IContextMenu *iface,
                                                 LPCMINVOKECOMMANDINFO info)
{
    return E_NOTIMPL;
}

static HRESULT WINAPI contextmenu_GetCommandString(IContextMenu *iface, UINT_PTR command,
                                                    UINT type, UINT *reserved, LPSTR name,
                                                    UINT max)
{
    return E_INVALIDARG;
}

static const IContextMenuVtbl contextmenu_vtbl =
{
    contextmenu_QueryInterface,
    contextmenu_AddRef,
    contextmenu_Release,
    contextmenu_QueryContextMenu,
    contextmenu_InvokeCommand,
    contextmenu_GetCommandString
};

/* IShellExtInit */

static HRESULT WINAPI shellextinit_QueryInterface(IShellExtInit *iface, REFIID iid, void **out)
{
    return irshell_query_interface(impl_from_IShellExtInit(iface), iid, out);
}

static ULONG WINAPI shellextinit_AddRef(IShellExtInit *iface)
{
    return irshell_addref(impl_from_IShellExtInit(iface));
}

static ULONG WINAPI shellextinit_Release(IShellExtInit *iface)
{
    return irshell_release(impl_from_IShellExtInit(iface));
}

static HRESULT WINAPI shellextinit_Initialize(IShellExtInit *iface, LPCITEMIDLIST folder,
                                              IDataObject *data_object, HKEY progid)
{
    TRACE("(%p, %p, %p, %p)\n", iface, folder, data_object, progid);
    return S_OK;
}

static const IShellExtInitVtbl shellextinit_vtbl =
{
    shellextinit_QueryInterface,
    shellextinit_AddRef,
    shellextinit_Release,
    shellextinit_Initialize
};

static HRESULT irshell_create(IUnknown *outer, REFIID iid, void **out)
{
    struct irshell *object;
    HRESULT hr;

    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;

    if (!(object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object))))
        return E_OUTOFMEMORY;

    object->IShellFolder_iface.lpVtbl = &shellfolder_vtbl;
    object->IPersistFolder_iface.lpVtbl = &persistfolder_vtbl;
    object->IContextMenu_iface.lpVtbl = &contextmenu_vtbl;
    object->IShellExtInit_iface.lpVtbl = &shellextinit_vtbl;
    object->ref = 1;
    InterlockedIncrement(&object_count);

    hr = irshell_query_interface(object, iid, out);
    irshell_release(object);
    return hr;
}

/* IClassFactory */

static HRESULT WINAPI classfactory_QueryInterface(IClassFactory *iface, REFIID iid, void **out)
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

static ULONG WINAPI classfactory_AddRef(IClassFactory *iface)
{
    struct class_factory *This = impl_from_IClassFactory(iface);
    return InterlockedIncrement(&This->ref);
}

static ULONG WINAPI classfactory_Release(IClassFactory *iface)
{
    struct class_factory *This = impl_from_IClassFactory(iface);
    ULONG ref = InterlockedDecrement(&This->ref);

    if (!ref)
    {
        HeapFree(GetProcessHeap(), 0, This);
        InterlockedDecrement(&object_count);
    }
    return ref;
}

static HRESULT WINAPI classfactory_CreateInstance(IClassFactory *iface, IUnknown *outer,
                                                   REFIID iid, void **out)
{
    return irshell_create(outer, iid, out);
}

static HRESULT WINAPI classfactory_LockServer(IClassFactory *iface, BOOL lock)
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

static const IClassFactoryVtbl classfactory_vtbl =
{
    classfactory_QueryInterface,
    classfactory_AddRef,
    classfactory_Release,
    classfactory_CreateInstance,
    classfactory_LockServer
};

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    struct class_factory *factory;
    HRESULT hr;

    TRACE("(%s, %s, %p)\n", debugstr_guid(clsid), debugstr_guid(iid), out);

    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(clsid, &CLSID_InfraredRecipient))
        return CLASS_E_CLASSNOTAVAILABLE;

    if (!(factory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*factory))))
        return E_OUTOFMEMORY;

    factory->IClassFactory_iface.lpVtbl = &classfactory_vtbl;
    factory->ref = 1;
    InterlockedIncrement(&object_count);

    hr = IClassFactory_QueryInterface(&factory->IClassFactory_iface, iid, out);
    IClassFactory_Release(&factory->IClassFactory_iface);
    return hr;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return object_count || server_locks ? S_FALSE : S_OK;
}
