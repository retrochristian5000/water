/*
 * Historical NT edition profile tests.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "winternl.h"
#include "wine/test.h"

static const struct
{
    const WCHAR *name;
    DWORD major, minor, build, product;
    WORD sp;
} profiles[] =
{
    {L"nt351",       3, 51, 1057, VER_NT_WORKSTATION, 5},
    {L"nt351server", 3, 51, 1057, VER_NT_SERVER, 5},
    {L"nt40",        4, 0, 1381, VER_NT_WORKSTATION, 6},
    {L"nt40server",  4, 0, 1381, VER_NT_SERVER, 6},
    {L"win2k",       5, 0, 2195, VER_NT_WORKSTATION, 4},
    {L"win2kserver", 5, 0, 2195, VER_NT_SERVER, 4},
};

static void check_profile(unsigned int i)
{
    RTL_OSVERSIONINFOEXW version = {sizeof(version)};
    OSVERSIONINFOEXW winver = {sizeof(winver)};
    DWORD type = 0;
    NTSTATUS status;
    BOOL ret;

    if(i >= ARRAY_SIZE(profiles)) return;
    status = RtlGetVersion(&version);
    ok(!status, "RtlGetVersion returned %#lx.\n", status);
    ok(version.dwMajorVersion == profiles[i].major && version.dwMinorVersion == profiles[i].minor
            && version.dwBuildNumber == profiles[i].build, "Unexpected NT version %lu.%lu.%lu.\n",
            version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
    ok(version.dwPlatformId == VER_PLATFORM_WIN32_NT, "Unexpected platform %lu.\n", version.dwPlatformId);
    ok(version.wServicePackMajor == profiles[i].sp, "Unexpected service pack %u.\n", version.wServicePackMajor);
    ok(version.wProductType == profiles[i].product, "Unexpected product type %u.\n", version.wProductType);
    ret = RtlGetNtProductType(&type);
    ok(ret && type == profiles[i].product, "RtlGetNtProductType returned %d, %lu.\n", ret, type);
    ret = GetVersionExW((OSVERSIONINFOW *)&winver);
    ok(ret && winver.wProductType == profiles[i].product, "GetVersionExW returned %d, %u.\n",
            ret, winver.wProductType);
}

START_TEST(version)
{
    WCHAR self[MAX_PATH], dir[MAX_PATH], temp[MAX_PATH], child[MAX_PATH + 4];
    WCHAR keyname[MAX_PATH + 64], command[2 * MAX_PATH + 64];
    PROCESS_INFORMATION pi;
    STARTUPINFOW si = {sizeof(si)};
    DWORD disposition, size, wait, exit_code;
    unsigned int i;
    char **argv;
    int argc;
    LONG err;
    HKEY key;
    BOOL ret;

    argc = winetest_get_mainargs(&argv);
    if(argc == 3) {
        check_profile(atoi(argv[2]));
        return;
    }
    if(strcmp(winetest_platform, "wine")) {
        skip("WATER-specific version profiles.\n");
        return;
    }
    size = GetModuleFileNameW(NULL, self, ARRAY_SIZE(self));
    if(!size || size >= ARRAY_SIZE(self)) return;
    size = GetTempPathW(ARRAY_SIZE(dir), dir);
    if(!size || size >= ARRAY_SIZE(dir)) return;
    if(!GetTempFileNameW(dir, L"ntv", 0, temp)) {
        ok(0, "Temporary name failed: %lu.\n", GetLastError());
        return;
    }
    swprintf(child, ARRAY_SIZE(child), L"%s.exe", temp);
    if(!CopyFileW(self, child, TRUE)) {
        ok(0, "Temporary test copy failed: %lu.\n", GetLastError());
        DeleteFileW(temp);
        return;
    }
    swprintf(keyname, ARRAY_SIZE(keyname), L"Software\\Wine\\AppDefaults\\%s", wcsrchr(child, '\\') + 1);
    err = RegCreateKeyExW(HKEY_CURRENT_USER, keyname, 0, NULL, 0, KEY_SET_VALUE, NULL, &key, &disposition);
    ok(!err, "Creating test profile failed: %ld.\n", err);
    if(err) goto done;
    if(disposition != REG_CREATED_NEW_KEY) {
        skip("Temporary executable already has a profile; leaving it unchanged.\n");
        RegCloseKey(key);
        goto done;
    }

    for(i = 0; i < ARRAY_SIZE(profiles); ++i) {
        winetest_push_context("%s", wine_dbgstr_w(profiles[i].name));
        err = RegSetValueExW(key, L"Version", 0, REG_SZ, (const BYTE *)profiles[i].name,
                (wcslen(profiles[i].name) + 1) * sizeof(WCHAR));
        ok(!err, "Setting version failed: %ld.\n", err);
        if(!err) {
            swprintf(command, ARRAY_SIZE(command), L"\"%s\" version %u", child, i);
            ret = CreateProcessW(child, command, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
            ok(ret, "Starting profile test failed: %lu.\n", GetLastError());
            if(ret) {
                wait = WaitForSingleObject(pi.hProcess, 30000);
                ok(wait == WAIT_OBJECT_0, "Child wait returned %lu.\n", wait);
                if(wait != WAIT_OBJECT_0) {
                    TerminateProcess(pi.hProcess, 1);
                    WaitForSingleObject(pi.hProcess, 5000);
                }else {
                    ret = GetExitCodeProcess(pi.hProcess, &exit_code);
                    ok(ret && !exit_code, "Profile check failed: %d, %lu.\n", ret, exit_code);
                }
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
            }
        }
        winetest_pop_context();
    }
    RegCloseKey(key);
    err = RegDeleteKeyW(HKEY_CURRENT_USER, keyname);
    ok(!err, "Deleting temporary profile failed: %ld.\n", err);
done:
    DeleteFileW(child);
    DeleteFileW(temp);
}
