/*
 * Tests for the Infrared Recipient shell extension
 *
 * Copyright 2026 Vincent and the Wine project
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
#include "objbase.h"
#include "shlobj.h"

#include "wine/test.h"

static const CLSID CLSID_InfraredRecipient =
    {0x00435ae0,0xbffb,0x11cf,{0xa9,0xd8,0x00,0xaa,0x00,0x42,0x35,0x96}};

static void test_class_object(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *can_unload)(void);
    IClassFactory *factory;
    IShellFolder *folder;
    IPersistFolder *persist;
    IContextMenu *menu;
    IShellExtInit *init;
    IEnumIDList *enumerator;
    LPITEMIDLIST item = (void *)0xdeadbeef;
    CLSID clsid;
    ULONG fetched = 0xdeadbeef;
    SFGAOF attributes = SFGAO_FILESYSTEM | SFGAO_DROPTARGET;
    HMODULE module;
    HRESULT hr;
    void *obj = (void *)0xdeadbeef;

    module = LoadLibraryA("irshell.dll");
    ok(module != NULL, "Failed to load irshell.dll, error %lu.\n", GetLastError());
    if (!module) return;

    get_class_object = (void *)GetProcAddress(module, "DllGetClassObject");
    can_unload = (void *)GetProcAddress(module, "DllCanUnloadNow");
    ok(!!get_class_object, "DllGetClassObject export is missing.\n");
    ok(!!can_unload, "DllCanUnloadNow export is missing.\n");
    if (!get_class_object || !can_unload)
    {
        FreeLibrary(module);
        return;
    }

    hr = can_unload();
    ok(hr == S_OK, "Expected S_OK before activation, got %#lx.\n", hr);

    hr = get_class_object(&CLSID_InfraredRecipient, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK, "DllGetClassObject failed, hr %#lx.\n", hr);
    if (FAILED(hr))
    {
        FreeLibrary(module);
        return;
    }

    hr = can_unload();
    ok(hr == S_FALSE, "Expected S_FALSE while class factory is alive, got %#lx.\n", hr);

    hr = IClassFactory_CreateInstance(factory, (IUnknown *)0xdeadbeef, &IID_IUnknown, &obj);
    ok(hr == CLASS_E_NOAGGREGATION, "Expected CLASS_E_NOAGGREGATION, got %#lx.\n", hr);
    ok(obj == NULL, "Expected NULL object on aggregation failure, got %p.\n", obj);

    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IShellFolder, (void **)&folder);
    ok(hr == S_OK, "Failed to create IShellFolder, hr %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IShellFolder_QueryInterface(folder, &IID_IPersistFolder, (void **)&persist);
        ok(hr == S_OK, "IPersistFolder query failed, hr %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            hr = IPersistFolder_GetClassID(persist, &clsid);
            ok(hr == S_OK, "GetClassID failed, hr %#lx.\n", hr);
            ok(IsEqualGUID(&clsid, &CLSID_InfraredRecipient), "Unexpected class ID.\n");
            IPersistFolder_Release(persist);
        }

        hr = IShellFolder_QueryInterface(folder, &IID_IContextMenu, (void **)&menu);
        ok(hr == S_OK, "IContextMenu query failed, hr %#lx.\n", hr);
        if (SUCCEEDED(hr)) IContextMenu_Release(menu);

        hr = IShellFolder_QueryInterface(folder, &IID_IShellExtInit, (void **)&init);
        ok(hr == S_OK, "IShellExtInit query failed, hr %#lx.\n", hr);
        if (SUCCEEDED(hr)) IShellExtInit_Release(init);

        hr = IShellFolder_GetAttributesOf(folder, 0, NULL, &attributes);
        ok(hr == S_OK, "GetAttributesOf failed, hr %#lx.\n", hr);
        ok(attributes == SFGAO_FILESYSTEM, "Unexpected root attributes %#lx.\n", attributes);

        hr = IShellFolder_EnumObjects(folder, NULL, SHCONTF_FOLDERS | SHCONTF_NONFOLDERS, &enumerator);
        ok(hr == S_OK, "EnumObjects failed, hr %#lx.\n", hr);
        if (SUCCEEDED(hr))
        {
            hr = IEnumIDList_Next(enumerator, 1, &item, &fetched);
            ok(hr == S_FALSE, "Expected an empty enumeration, got %#lx.\n", hr);
            ok(item == NULL, "Expected no PIDL, got %p.\n", item);
            ok(fetched == 0, "Expected zero items, got %lu.\n", fetched);
            IEnumIDList_Release(enumerator);
        }

        IShellFolder_Release(folder);
    }

    IClassFactory_Release(factory);

    hr = can_unload();
    ok(hr == S_OK, "Expected S_OK after releasing all objects, got %#lx.\n", hr);

    FreeLibrary(module);
}

START_TEST(irshell)
{
    test_class_object();
}
