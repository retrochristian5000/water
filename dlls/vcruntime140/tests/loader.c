/*
 * Tests for the MSVC VCRUNTIME Windows loader wrappers.
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "windef.h"
#include "winbase.h"
#include "wine/test.h"

static DWORD (CDECL *p_vcrt_GetModuleFileNameW)(HMODULE, WCHAR *, DWORD);
static HMODULE (CDECL *p_vcrt_GetModuleHandleW)(const WCHAR *);
static HMODULE (CDECL *p_vcrt_LoadLibraryExW)(const WCHAR *, HANDLE, DWORD);

static void test_loader_wrappers(void)
{
    static const WCHAR kernel32_name[] = {'k','e','r','n','e','l','3','2','.','d','l','l',0};
    static const WCHAR missing_name[] = {'w','i','n','e','_','v','c','r','t','_',
                                         'n','o','t','_','p','r','e','s','e','n','t','.','d','l','l',0};
    WCHAR expected[MAX_PATH], actual[MAX_PATH];
    HMODULE expected_module, module;
    DWORD expected_len, actual_len, error;

    expected_len = GetModuleFileNameW(NULL, expected, ARRAY_SIZE(expected));
    actual_len = p_vcrt_GetModuleFileNameW(NULL, actual, ARRAY_SIZE(actual));
    ok(expected_len > 0 && expected_len < ARRAY_SIZE(expected),
       "GetModuleFileNameW returned %lu\n", expected_len);
    ok(actual_len == expected_len, "expected path length %lu, got %lu\n",
       expected_len, actual_len);
    if (expected_len && actual_len && actual_len < ARRAY_SIZE(actual))
        ok(!lstrcmpW(expected, actual), "VCRUNTIME executable path differs from kernel32\n");

    expected_module = GetModuleHandleW(NULL);
    module = p_vcrt_GetModuleHandleW(NULL);
    ok(module == expected_module, "expected process module %p, got %p\n",
       expected_module, module);

    expected_module = GetModuleHandleW(kernel32_name);
    module = p_vcrt_GetModuleHandleW(kernel32_name);
    ok(module == expected_module, "expected kernel32 handle %p, got %p\n",
       expected_module, module);

    module = p_vcrt_LoadLibraryExW(kernel32_name, NULL, 0);
    ok(module == expected_module, "expected loaded kernel32 handle %p, got %p\n",
       expected_module, module);
    if (module) FreeLibrary(module);

    SetLastError(0xdeadbeef);
    module = p_vcrt_LoadLibraryExW(missing_name, NULL, 0);
    error = GetLastError();
    ok(!module, "unexpected nonexistent module handle %p\n", module);
    ok(error == ERROR_MOD_NOT_FOUND, "expected ERROR_MOD_NOT_FOUND, got %lu\n", error);
    if (module) FreeLibrary(module);
}

START_TEST(loader)
{
    HMODULE vcruntime = LoadLibraryA("vcruntime140.dll");

    if (!vcruntime)
    {
        win_skip("vcruntime140.dll is not installed\n");
        return;
    }

    p_vcrt_GetModuleFileNameW = (void *)GetProcAddress(vcruntime, "__vcrt_GetModuleFileNameW");
    p_vcrt_GetModuleHandleW = (void *)GetProcAddress(vcruntime, "__vcrt_GetModuleHandleW");
    p_vcrt_LoadLibraryExW = (void *)GetProcAddress(vcruntime, "__vcrt_LoadLibraryExW");

    ok(!!p_vcrt_GetModuleFileNameW, "missing __vcrt_GetModuleFileNameW export\n");
    ok(!!p_vcrt_GetModuleHandleW, "missing __vcrt_GetModuleHandleW export\n");
    ok(!!p_vcrt_LoadLibraryExW, "missing __vcrt_LoadLibraryExW export\n");

    if (p_vcrt_GetModuleFileNameW && p_vcrt_GetModuleHandleW && p_vcrt_LoadLibraryExW)
        test_loader_wrappers();

    FreeLibrary(vcruntime);
}
