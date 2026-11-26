/*
 * Legacy MediaPlayer adapter tests
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#define COBJMACROS
#include <stdarg.h>
#include "windows.h"
#include "ole2.h"
#include "wine/test.h"

static const CLSID legacy_class = {0x22d6f312,0xb0f6,0x11d0,{0x94,0xab,0x00,0x80,0xc7,0x4c,0x7e,0x95}};
static const IID legacy_iid = {0x22d6f311,0xb0f6,0x11d0,{0x94,0xab,0x00,0x80,0xc7,0x4c,0x7e,0x95}};

static void test_property(IDispatch *player, WCHAR *name, VARIANT *input)
{
    DISPID id, put_id = DISPID_PROPERTYPUT;
    DISPPARAMS params = {input, &put_id, 1, 1};
    VARIANT output;
    HRESULT hr;

    hr = IDispatch_GetIDsOfNames(player, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &id);
    ok(hr == S_OK, "Name lookup returned %#lx\n", hr);
    if(FAILED(hr)) return;
    hr = IDispatch_Invoke(player, id, &IID_NULL, LOCALE_USER_DEFAULT,
            DISPATCH_PROPERTYPUT, &params, NULL, NULL, NULL);
    ok(hr == S_OK, "Property put returned %#lx\n", hr);
    memset(&params, 0, sizeof(params));
    VariantInit(&output);
    hr = IDispatch_Invoke(player, id, &IID_NULL, LOCALE_USER_DEFAULT,
            DISPATCH_PROPERTYGET, &params, &output, NULL, NULL);
    ok(hr == S_OK && V_VT(&output) == V_VT(input), "Property get: %#lx, type %u\n", hr, V_VT(&output));
    if(hr == S_OK && V_VT(&output) == V_VT(input)) {
        if(V_VT(input) == VT_BOOL)
            ok(V_BOOL(&output) == V_BOOL(input), "Boolean did not round trip\n");
        else
            ok(V_BSTR(&output) && !lstrcmpW(V_BSTR(&output), V_BSTR(input)), "File name did not round trip\n");
    }
    VariantClear(&output);
}

START_TEST(msdxm)
{
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *can_unload)(void);
    HMODULE module;
    IClassFactory *factory;
    IDispatch *player;
    void *out;
    HRESULT hr;
    VARIANT input;
    UINT count;

    if(strcmp(winetest_platform, "wine")) {
        skip("WATER partial adapter contract, not native ABI tests.\n");
        return;
    }
    hr = CoInitialize(NULL);
    ok(SUCCEEDED(hr), "CoInitialize returned %#lx\n", hr);
    if(FAILED(hr)) return;
    module = LoadLibraryW(L"msdxm.dll");
    ok(!!module, "LoadLibrary failed: %lu\n", GetLastError());
    if(!module) goto uninit;
    get_class = (void *)GetProcAddress(module, "DllGetClassObject");
    can_unload = (void *)GetProcAddress(module, "DllCanUnloadNow");
    ok(get_class && can_unload, "Missing exports\n");
    if(!get_class || !can_unload) goto done;

    ok(can_unload() == S_OK, "Initial unload failed\n");
    out = (void *)0xdeadbeef;
    hr = get_class(&GUID_NULL, &IID_IClassFactory, &out);
    ok(hr == CLASS_E_CLASSNOTAVAILABLE && !out, "Unknown class returned %#lx, %p\n", hr, out);
    hr = get_class(&legacy_class, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK, "Legacy factory returned %#lx\n", hr);
    if(FAILED(hr)) goto done;
    ok(can_unload() == S_FALSE, "Factory not counted\n");
    out = (void *)0xdeadbeef;
    hr = IClassFactory_CreateInstance(factory, NULL, &legacy_iid, &out);
    ok(hr == E_NOINTERFACE && !out, "Incomplete legacy vtable exposed: %#lx, %p\n", hr, out);

    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IDispatch, (void **)&player);
    ok(hr == S_OK, "Adapter creation returned %#lx\n", hr);
    IClassFactory_Release(factory);
    if(FAILED(hr)) goto done;
    ok(can_unload() == S_FALSE, "Live object not counted\n");
    count = 1;
    hr = IDispatch_GetTypeInfoCount(player, &count);
    ok(hr == S_OK && !count, "Unexpected type info count %u, %#lx\n", count, hr);
    VariantInit(&input);
    V_VT(&input) = VT_BOOL;
    V_BOOL(&input) = VARIANT_FALSE;
    test_property(player, L"AutoStart", &input);
    V_VT(&input) = VT_BSTR;
    V_BSTR(&input) = SysAllocString(L"example.wmv");
    if(V_BSTR(&input)) test_property(player, L"FileName", &input);
    VariantClear(&input);
    IDispatch_Release(player);
    ok(can_unload() == S_OK, "Released adapter still counted\n");

done:
    FreeLibrary(module);
uninit:
    CoUninitialize();
}
