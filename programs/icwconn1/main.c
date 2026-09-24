/*
 * Internet Connection Wizard compatibility frontend
 *
 * Clean-room compatibility implementation of ICWCONN1.EXE.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS

#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shellapi.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(icwconn1);

static const WCHAR icw_keyW[] = L"Software\\Microsoft\\Internet Connection Wizard";
static const WCHAR link_nameW[] = L"Connect to the Internet.lnk";

static void set_icw_dword(const WCHAR *name, DWORD value)
{
    HKEY key;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, icw_keyW, 0, NULL, 0, KEY_SET_VALUE,
                        NULL, &key, NULL) == ERROR_SUCCESS)
    {
        RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
        RegCloseKey(key);
    }
}

static DWORD get_icw_dword(const WCHAR *name)
{
    DWORD type, value = 0, size = sizeof(value);
    HKEY key;

    if (RegOpenKeyExW(HKEY_CURRENT_USER, icw_keyW, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return 0;

    if (RegQueryValueExW(key, name, NULL, &type, (BYTE *)&value, &size) != ERROR_SUCCESS ||
        type != REG_DWORD)
        value = 0;

    RegCloseKey(key);
    return value;
}

static BOOL get_desktop_link_path(WCHAR *path, DWORD count)
{
    size_t len;

    if (FAILED(SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE,
                                NULL, SHGFP_TYPE_CURRENT, path)))
        return FALSE;

    len = lstrlenW(path);
    if (len + 1 + ARRAY_SIZE(link_nameW) > count) return FALSE;

    if (len && path[len - 1] != '\\') lstrcatW(path, L"\\");
    lstrcatW(path, link_nameW);
    return TRUE;
}

static BOOL create_desktop_link(void)
{
    IShellLinkW *link = NULL;
    IPersistFile *persist = NULL;
    WCHAR link_path[MAX_PATH], exe_path[MAX_PATH];
    HRESULT hr;

    if (!get_desktop_link_path(link_path, ARRAY_SIZE(link_path)) ||
        !GetModuleFileNameW(NULL, exe_path, ARRAY_SIZE(exe_path)))
        return FALSE;

    hr = CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IShellLinkW, (void **)&link);
    if (FAILED(hr)) return FALSE;

    hr = IShellLinkW_SetPath(link, exe_path);
    if (SUCCEEDED(hr)) hr = IShellLinkW_SetArguments(link, L"/icon");
    if (SUCCEEDED(hr)) hr = IShellLinkW_SetDescription(link, L"Connect to the Internet");
    if (SUCCEEDED(hr))
        hr = IShellLinkW_QueryInterface(link, &IID_IPersistFile, (void **)&persist);
    if (SUCCEEDED(hr)) hr = IPersistFile_Save(persist, link_path, TRUE);

    if (persist) IPersistFile_Release(persist);
    IShellLinkW_Release(link);

    if (SUCCEEDED(hr)) set_icw_dword(L"DesktopChanged", 1);
    return SUCCEEDED(hr);
}

static BOOL remove_desktop_link(void)
{
    WCHAR link_path[MAX_PATH];

    if (!get_desktop_link_path(link_path, ARRAY_SIZE(link_path))) return FALSE;
    if (!DeleteFileW(link_path) && GetLastError() != ERROR_FILE_NOT_FOUND) return FALSE;

    set_icw_dword(L"DesktopChanged", 0);
    return TRUE;
}

static BOOL launch_connections(void)
{
    SHELLEXECUTEINFOW info = {sizeof(info)};
    DWORD result;

    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpFile = L"control.exe";
    info.lpParameters = L"inetcpl.cpl,,4";
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) return FALSE;

    if (info.hProcess)
    {
        WaitForSingleObject(info.hProcess, INFINITE);
        CloseHandle(info.hProcess);
    }

    result = (DWORD_PTR)info.hInstApp;
    return result > 32;
}

static BOOL launch_shell_next(int argc, WCHAR **argv, int index)
{
    WCHAR params[2048] = L"";
    int i;

    if (index + 1 >= argc) return FALSE;

    for (i = index + 2; i < argc; ++i)
    {
        size_t used = lstrlenW(params);
        size_t need = lstrlenW(argv[i]) + 4;

        if (used + need >= ARRAY_SIZE(params)) break;
        if (used) lstrcatW(params, L" ");
        lstrcatW(params, L"\"");
        lstrcatW(params, argv[i]);
        lstrcatW(params, L"\"");
    }

    return (INT_PTR)ShellExecuteW(NULL, L"open", argv[index + 1],
                                  params[0] ? params : NULL, NULL, SW_SHOWNORMAL) > 32;
}

int __cdecl wmain(int argc, WCHAR **argv)
{
    BOOL shell_next = FALSE;
    int shell_next_index = -1;
    HRESULT hr;
    int i, ret = 0;

    hr = CoInitialize(NULL);
    if (FAILED(hr)) return 1;

    for (i = 1; i < argc; ++i)
    {
        if (!lstrcmpiW(argv[i], L"/desktop"))
        {
            ret = !create_desktop_link();
            goto done;
        }
        if (!lstrcmpiW(argv[i], L"/restoredesktop"))
        {
            ret = !remove_desktop_link();
            goto done;
        }
        if (!lstrcmpiW(argv[i], L"/smartstart") && get_icw_dword(L"Completed"))
            goto done;
        if (!lstrcmpiW(argv[i], L"/starturl") && i + 1 < argc)
        {
            ret = (INT_PTR)ShellExecuteW(NULL, L"open", argv[++i], NULL, NULL, SW_SHOWNORMAL) <= 32;
            goto done;
        }
        if (!lstrcmpiW(argv[i], L"/shellnext") && i + 1 < argc)
        {
            shell_next = TRUE;
            shell_next_index = i;
            break;
        }
    }

    /*
     * The historical ICW performs ISP signup, DUN provisioning and branding.
     * Water does not yet implement that private stack, so expose the existing
     * Internet Properties connection UI instead of reporting fake success.
     */
    if (!launch_connections())
    {
        ERR("failed to launch Internet connection settings\n");
        ret = 1;
        goto done;
    }

    if (shell_next && !launch_shell_next(argc, argv, shell_next_index)) ret = 1;

done:
    CoUninitialize();
    return ret;
}
