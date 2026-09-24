/*
 * DVD Play placeholder application
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program preserves the modern Windows DVDPLAY.EXE compatibility
 * surface by locating Windows Media Player through its App Paths
 * registration and launching it.
 */

#include <windows.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dvdplay);

static BOOL find_wmplayer(WCHAR *filename, DWORD count)
{
    static const WCHAR app_path[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wmplayer.exe";
    WCHAR path[MAX_PATH];
    DWORD len, size = sizeof(path);
    LONG ret;

    ret = RegGetValueW(HKEY_LOCAL_MACHINE, app_path, L"Path",
                       RRF_RT_REG_SZ | RRF_RT_REG_EXPAND_SZ, NULL, path, &size);
    if (ret != ERROR_SUCCESS)
    {
        WARN("wmplayer App Paths registration not found, error %ld\n", ret);
        return FALSE;
    }

    len = SearchPathW(path, L"wmplayer.exe", NULL, count, filename, NULL);
    if (!len || len >= count)
    {
        WARN("wmplayer.exe not found in %s\n", debugstr_w(path));
        return FALSE;
    }

    return TRUE;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION process;
    WCHAR filename[MAX_PATH];

    (void)argc;
    (void)argv;

    if (!find_wmplayer(filename, ARRAY_SIZE(filename))) return 0;

    if (!CreateProcessW(filename, NULL, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
    {
        WARN("failed to start %s, error %lu\n", debugstr_w(filename), GetLastError());
        return 0;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
