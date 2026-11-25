/*
 * Microsoft VM compatibility boundary
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

START_TEST(msjava)
{
    HMODULE module;
    INT (WINAPI *get_created)(void **, INT, INT *);
    INT (WINAPI *get_defaults)(void *);
    INT (WINAPI *create_vm)(void **, void **, void *);
    HRESULT (WINAPI *can_unload)(void);
    HRESULT (WINAPI *get_class)(REFCLSID, REFIID, void **);
    static const GUID unknown = {0};
    void *vm, *env, *slots[2], *obj;
    INT count, version, ret;
    HRESULT hr;

    if(strcmp(winetest_platform, "wine")) {
        skip("WATER no-backend contract; native Microsoft VM may execute Java.\n");
        return;
    }
    module = LoadLibraryW(L"msjava.dll");
    ok(module != NULL, "LoadLibrary failed: %lu\n", GetLastError());
    if(!module)
        return;

    get_created = (void *)GetProcAddress(module, "JNI_GetCreatedJavaVMs");
    get_defaults = (void *)GetProcAddress(module, "JNI_GetDefaultJavaVMInitArgs");
    create_vm = (void *)GetProcAddress(module, "JNI_CreateJavaVM");
    can_unload = (void *)GetProcAddress(module, "DllCanUnloadNow");
    get_class = (void *)GetProcAddress(module, "DllGetClassObject");
    ok(get_created && get_defaults && create_vm && can_unload && get_class,
       "Missing MSJAVA exports\n");
    if(!get_created || !get_defaults || !create_vm || !can_unload || !get_class)
        goto done;

    count = -1;
    ret = get_created(NULL, 0, &count);
    ok(ret == 0 && count == 0, "Enumeration returned %d, count %d\n", ret, count);
    slots[0] = slots[1] = (void *)0xdeadbeef;
    count = -1;
    ret = get_created(slots, 2, &count);
    ok(ret == 0 && !count, "Enumeration returned %d, count %d\n", ret, count);
    ok(slots[0] == (void *)0xdeadbeef && slots[1] == (void *)0xdeadbeef,
       "Unused slots changed\n");
    ret = get_created(NULL, 1, &count);
    ok(ret == -1, "Invalid enumeration returned %d\n", ret);
    ret = get_created(slots, -1, &count);
    ok(ret == -1, "Negative capacity returned %d\n", ret);
    ret = get_created(NULL, 0, NULL);
    ok(ret == -1, "NULL count returned %d\n", ret);

    version = 0x00010001;
    ret = get_defaults(&version);
    ok(ret == -3 && version == 0x00010001, "Default args returned %d, version %#x\n", ret, version);
    ret = get_defaults(NULL);
    ok(ret == -1, "NULL args returned %d\n", ret);

    vm = env = (void *)0xdeadbeef;
    ret = create_vm(&vm, &env, &version);
    ok(ret == -1 && !vm && !env, "Create returned %d, vm %p, env %p\n", ret, vm, env);
    count = -1;
    ret = get_created(NULL, 0, &count);
    ok(ret == 0 && !count, "Failed creation left a VM: %d, count %d\n", ret, count);
    hr = can_unload();
    ok(hr == S_OK, "DllCanUnloadNow returned %#lx\n", hr);
    obj = (void *)0xdeadbeef;
    hr = get_class(&unknown, &unknown, &obj);
    ok(hr == CLASS_E_CLASSNOTAVAILABLE && !obj, "GetClassObject returned %#lx, %p\n", hr, obj);

done:
    FreeLibrary(module);
}
