/*
 * Tests for the Shockwave Flash compatibility control
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
#include "ole2.h"
#include "oleauto.h"
#include "olectl.h"

#include "wine/test.h"

#define SWFLASH_VERSION 0x0005002c

static const CLSID CLSID_ShockwaveFlash =
    {0xd27cdb6e,0xae6d,0x11cf,{0x96,0xb8,0x44,0x45,0x53,0x54,0x00,0x00}};

static void test_control(void)
{
    HRESULT (WINAPI *get_class_object)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *can_unload)(void);
    IClassFactory *factory;
    IPersistStreamInit *persist;
    IOleObject *ole_object;
    IDispatch *dispatch;
    DISPPARAMS params = {0};
    DISPID dispid, property_put = DISPID_PROPERTYPUT;
    VARIANT result, value;
    OLECHAR *name;
    HMODULE module;
    HRESULT hr;
    void *object = (void *)0xdeadbeef;

    module = LoadLibraryA("swflash.ocx");
    ok(module != NULL, "Failed to load swflash.ocx, error %lu.\n", GetLastError());
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

    hr = get_class_object(&CLSID_ShockwaveFlash, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK, "DllGetClassObject failed, hr %#lx.\n", hr);
    if (FAILED(hr))
    {
        FreeLibrary(module);
        return;
    }

    hr = can_unload();
    ok(hr == S_FALSE, "Expected S_FALSE while factory is alive, got %#lx.\n", hr);

    hr = IClassFactory_CreateInstance(factory, (IUnknown *)0xdeadbeef, &IID_IUnknown, &object);
    ok(hr == CLASS_E_NOAGGREGATION, "Expected CLASS_E_NOAGGREGATION, got %#lx.\n", hr);
    ok(object == NULL, "Expected NULL object on aggregation failure, got %p.\n", object);

    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IDispatch, (void **)&dispatch);
    ok(hr == S_OK, "Failed to create IDispatch, hr %#lx.\n", hr);
    IClassFactory_Release(factory);
    if (FAILED(hr))
    {
        FreeLibrary(module);
        return;
    }

    hr = IDispatch_QueryInterface(dispatch, &IID_IOleObject, (void **)&ole_object);
    ok(hr == S_OK, "IOleObject query failed, hr %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        CLSID clsid;
        DWORD status;

        hr = IOleObject_GetUserClassID(ole_object, &clsid);
        ok(hr == S_OK, "GetUserClassID failed, hr %#lx.\n", hr);
        ok(IsEqualGUID(&clsid, &CLSID_ShockwaveFlash), "Unexpected user class ID.\n");

        hr = IOleObject_GetMiscStatus(ole_object, DVASPECT_CONTENT, &status);
        ok(hr == S_OK, "GetMiscStatus failed, hr %#lx.\n", hr);
        ok(status == 131473, "Unexpected misc status %lu.\n", status);

        IOleObject_Release(ole_object);
    }

    hr = IDispatch_QueryInterface(dispatch, &IID_IPersistStreamInit, (void **)&persist);
    ok(hr == S_OK, "IPersistStreamInit query failed, hr %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IPersistStreamInit_IsDirty(persist);
        ok(hr == S_FALSE, "Fresh object should not be dirty, hr %#lx.\n", hr);
        IPersistStreamInit_Release(persist);
    }

    name = L"FlashVersion";
    hr = IDispatch_GetIDsOfNames(dispatch, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid);
    ok(hr == S_OK, "FlashVersion GetIDsOfNames failed, hr %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        VariantInit(&result);
        hr = IDispatch_Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                              DISPATCH_METHOD, &params, &result, NULL, NULL);
        ok(hr == S_OK, "FlashVersion Invoke failed, hr %#lx.\n", hr);
        ok(V_VT(&result) == VT_I4, "Expected VT_I4, got %u.\n", V_VT(&result));
        if (V_VT(&result) == VT_I4)
            ok(V_I4(&result) == SWFLASH_VERSION, "Unexpected Flash version %#lx.\n", V_I4(&result));
        VariantClear(&result);
    }

    name = L"Movie";
    hr = IDispatch_GetIDsOfNames(dispatch, &IID_NULL, &name, 1, LOCALE_USER_DEFAULT, &dispid);
    ok(hr == S_OK, "Movie GetIDsOfNames failed, hr %#lx.\n", hr);
    if (SUCCEEDED(hr))
    {
        VariantInit(&value);
        V_VT(&value) = VT_BSTR;
        V_BSTR(&value) = SysAllocString(L"test.swf");
        ok(V_BSTR(&value) != NULL, "Failed to allocate test movie name.\n");

        params.rgvarg = &value;
        params.rgdispidNamedArgs = &property_put;
        params.cArgs = 1;
        params.cNamedArgs = 1;
        hr = IDispatch_Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                              DISPATCH_PROPERTYPUT, &params, NULL, NULL, NULL);
        ok(hr == S_OK, "Movie property put failed, hr %#lx.\n", hr);
        VariantClear(&value);

        params.rgvarg = NULL;
        params.rgdispidNamedArgs = NULL;
        params.cArgs = 0;
        params.cNamedArgs = 0;
        VariantInit(&result);
        hr = IDispatch_Invoke(dispatch, dispid, &IID_NULL, LOCALE_USER_DEFAULT,
                              DISPATCH_PROPERTYGET, &params, &result, NULL, NULL);
        ok(hr == S_OK, "Movie property get failed, hr %#lx.\n", hr);
        ok(V_VT(&result) == VT_BSTR, "Expected VT_BSTR, got %u.\n", V_VT(&result));
        if (V_VT(&result) == VT_BSTR)
            ok(!wcscmp(V_BSTR(&result), L"test.swf"), "Unexpected movie %s.\n",
               wine_dbgstr_w(V_BSTR(&result)));
        VariantClear(&result);
    }

    IDispatch_Release(dispatch);

    hr = can_unload();
    ok(hr == S_OK, "Expected S_OK after releasing all objects, got %#lx.\n", hr);

    FreeLibrary(module);
}

START_TEST(swflash)
{
    test_control();
}
