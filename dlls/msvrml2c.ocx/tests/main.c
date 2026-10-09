/*
 * VRML 2.0 ActiveX initial COM class factory contract.
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
#include "oleauto.h"
#include "wine/test.h"

static const GUID browser = {0x90a7533d, 0x88fe, 0x11d0,
    {0x9d, 0xbe, 0x00, 0x00, 0xc0, 0x41, 0x1f, 0xc3}};
static const GUID unknown_class = {0x12345678, 0, 0, {0, 0, 0, 0, 0, 0, 1, 0}};

START_TEST(main)
{
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *can_unload)(void);
    HRESULT (WINAPI *register_server)(void);
    HRESULT (WINAPI *unregister_server)(void);
    IClassFactory *factory = NULL;
    IUnknown *unknown = NULL;
    HMODULE module;
    HRESULT hr;
    void *out = (void *)0xdeadbeef;

    module = LoadLibraryA("msvrml2c.ocx");
    if (!module)
    {
        win_skip("msvrml2c.ocx is not available\n");
        return;
    }

    get_class = (void *)GetProcAddress(module, "DllGetClassObject");
    can_unload = (void *)GetProcAddress(module, "DllCanUnloadNow");
    register_server = (void *)GetProcAddress(module, "DllRegisterServer");
    unregister_server = (void *)GetProcAddress(module, "DllUnregisterServer");
    ok(get_class != NULL, "DllGetClassObject is missing\n");
    ok(can_unload != NULL, "DllCanUnloadNow is missing\n");
    ok(register_server != NULL, "DllRegisterServer is missing\n");
    ok(unregister_server != NULL, "DllUnregisterServer is missing\n");
    if (!get_class || !can_unload || !register_server || !unregister_server)
    {
        FreeLibrary(module);
        return;
    }

    ok(can_unload() == S_OK, "module starts with a live reference\n");
    hr = get_class(&browser, &IID_IClassFactory, NULL);
    ok(hr == E_POINTER, "NULL output returned %#lx\n", hr);

    hr = get_class(&unknown_class, &IID_IClassFactory, &out);
    ok(hr == CLASS_E_CLASSNOTAVAILABLE, "unknown class returned %#lx\n", hr);
    ok(out == NULL, "unknown class left stale output pointer %p\n", out);

    out = (void *)0xdeadbeef;
    hr = get_class(&browser, &IID_IDispatch, &out);
    ok(hr == E_NOINTERFACE, "unimplemented factory interface returned %#lx\n", hr);
    ok(out == NULL, "unsupported IID left stale pointer %p\n", out);
    ok(can_unload() == S_OK, "unsupported QI leaked factory\n");

    hr = get_class(&browser, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK, "viewer factory returned %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        ok(can_unload() == S_FALSE, "live factory does not prevent unload\n");

        hr = IClassFactory_QueryInterface(factory, &IID_IUnknown, (void **)&unknown);
        ok(hr == S_OK, "factory QI for IUnknown returned %#lx\n", hr);
        if (unknown) IUnknown_Release(unknown);

        hr = IClassFactory_CreateInstance(factory, (IUnknown *)factory,
                                           &IID_IUnknown, &out);
        ok(hr == CLASS_E_NOAGGREGATION, "aggregation returned %#lx\n", hr);

        out = (void *)0xdeadbeef;
        hr = IClassFactory_CreateInstance(factory, NULL, &IID_IUnknown, &out);
        ok(hr == E_NOTIMPL, "viewer instance must remain unsupported: %#lx\n", hr);
        ok(out == NULL, "unsupported viewer returned stale object %p\n", out);

        hr = IClassFactory_LockServer(factory, TRUE);
        ok(hr == S_OK, "LockServer(TRUE) returned %#lx\n", hr);
        ok(can_unload() == S_FALSE, "server lock does not prevent unload\n");
        hr = IClassFactory_LockServer(factory, FALSE);
        ok(hr == S_OK, "LockServer(FALSE) returned %#lx\n", hr);
        IClassFactory_Release(factory);
        ok(can_unload() == S_OK, "server lock or factory leaked\n");
    }

    /* Never register the unfinished viewer or overwrite a native viewer.
     * Both registration exports are intentionally non-destructive. */
    hr = register_server();
    ok(hr == SELFREG_E_CLASS, "unfinished viewer registered: %#lx\n", hr);
    hr = unregister_server();
    ok(hr == S_OK, "no-op unregistration returned %#lx\n", hr);

    FreeLibrary(module);
}
