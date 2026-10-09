/*
 * DirectAnimation (danim.dll) COM entry-point checks.
 *
 * Verify the factory ABI and preserve TODO coverage for viewer creation.
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
#include "wine/test.h"

static const GUID clsid_viewer = {0xb6ffc24c, 0x7e13, 0x11d0,
    {0x9b, 0x47, 0x00, 0xc0, 0x4f, 0xc2, 0xf5, 0x1d}};
static const GUID clsid_control = {0x69ad90ef, 0x1c20, 0x11d1,
    {0x88, 0x01, 0x00, 0xc0, 0x4f, 0xc2, 0x9d, 0x46}};
static const GUID unknown_class = {0x12345678, 0, 0, {0, 0, 0, 0, 0, 0, 0, 1}};

static void test_factory(HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **),
        HRESULT (WINAPI *can_unload)(void), const GUID *clsid)
{
    IClassFactory *factory = NULL;
    IUnknown *instance = NULL;
    void *unknown = (void *)0xdeadbeef;
    HRESULT hr;

    hr = get_class_object(clsid, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK, "DllGetClassObject returned %#lx\n", hr);
    if (FAILED(hr))
        return;

    if (can_unload)
        ok(can_unload() == S_FALSE, "factory not counted as a live object\n");

    hr = IClassFactory_QueryInterface(factory, &IID_IUnknown, &unknown);
    ok(hr == S_OK, "factory QueryInterface returned %#lx\n", hr);
    ok(unknown != NULL, "factory QueryInterface returned NULL\n");
    if (SUCCEEDED(hr))
        IUnknown_Release((IUnknown *)unknown);

    hr = IClassFactory_LockServer(factory, TRUE);
    ok(hr == S_OK, "LockServer(TRUE) returned %#lx\n", hr);

    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IUnknown, (void **)&instance);
    todo_wine ok(hr == S_OK, "DirectAnimation viewer is not implemented: %#lx\n", hr);
    if (SUCCEEDED(hr))
        IUnknown_Release(instance);

    hr = IClassFactory_LockServer(factory, FALSE);
    ok(hr == S_OK, "LockServer(FALSE) returned %#lx\n", hr);
    IClassFactory_Release(factory);
}

START_TEST(main)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *can_unload)(void);
    HMODULE module;
    HRESULT hr;
    void *out;

    module = LoadLibraryA("danim.dll");
    if (!module)
    {
        win_skip("danim.dll not present\n");
        return;
    }

    get_class_object = (void *)GetProcAddress(module, "DllGetClassObject");
    can_unload = (void *)GetProcAddress(module, "DllCanUnloadNow");
    ok(get_class_object != NULL, "missing DllGetClassObject export\n");
    ok(can_unload != NULL, "missing DllCanUnloadNow export\n");
    if (!get_class_object)
    {
        FreeLibrary(module);
        return;
    }

    out = (void *)0xdeadbeef;
    hr = get_class_object(&unknown_class, &IID_IClassFactory, &out);
    ok(hr == CLASS_E_CLASSNOTAVAILABLE, "unknown CLSID returned %#lx\n", hr);
    ok(!out, "unknown CLSID left stale pointer %p\n", out);

    test_factory(get_class_object, can_unload, &clsid_viewer);
    test_factory(get_class_object, can_unload, &clsid_control);

    if (can_unload)
        ok(can_unload() == S_OK, "factory or server lock leaked\n");

    FreeLibrary(module);
}
