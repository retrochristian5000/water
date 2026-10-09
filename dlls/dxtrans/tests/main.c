/*
 * DirectX Transform class-factory regression checks.
 *
 * DirectAnimation's ActiveX controls belong to danim.dll, not dxtrans.dll.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "wine/test.h"

static const GUID directanimation_control = {0x69ad90ef, 0x1c20, 0x11d1,
    {0x88, 0x01, 0x00, 0xc0, 0x4f, 0xc2, 0x9d, 0x46}};
static const GUID directanimation_viewer = {0xb6ffc24c, 0x7e13, 0x11d0,
    {0x9b, 0x47, 0x00, 0xc0, 0x4f, 0xc2, 0xf5, 0x1d}};
static const GUID classfactory_iid = {0x00000001, 0, 0,
    {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

static void test_unknown_classes(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    static const GUID *const unsupported[] =
    {
        &directanimation_control,
        &directanimation_viewer,
    };
    HMODULE module;
    HRESULT hr;
    void *object;
    unsigned int i;

    module = LoadLibraryA("dxtrans.dll");
    if (!module)
    {
        win_skip("dxtrans.dll is not available.\n");
        return;
    }

    get_class_object = (void *)GetProcAddress(module, "DllGetClassObject");
    if (!get_class_object)
    {
        win_skip("dxtrans.dll does not export DllGetClassObject.\n");
        FreeLibrary(module);
        return;
    }

    for (i = 0; i < ARRAY_SIZE(unsupported); i++)
    {
        object = (void *)0xdeadbeef;
        hr = get_class_object(unsupported[i], &classfactory_iid, &object);
        ok(hr == CLASS_E_CLASSNOTAVAILABLE, "unexpected HRESULT %#lx for class %u\n", hr, i);
        ok(!object, "class %u left a stale output pointer %p\n", i, object);
    }

    FreeLibrary(module);
}

START_TEST(main)
{
    test_unknown_classes();
}
