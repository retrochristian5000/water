/*
 * Quick Launch and the Windows 7+ unified pinned taskbar surface.
 *
 * IE4 Desktop Update introduced the optional Quick Launch toolbar to
 * Windows 95 and NT 4. Windows 7 retained pinned .lnk shortcuts but
 * unified their taskbar presentation with running application windows.
 *
 * WATER uses existing per-user shortcuts, never synthesizes Microsoft
 * product pins, and does not modify the foreign Windows taskband blobs.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <string.h>
#include <wchar.h>

#include "wine/debug.h"
#include "wine/win16_profile.h"
#include "explorer_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(explorer);

#define MAX_LAUNCHERS 24
#define OLD_LAUNCHER_SIZE 28
#define MODERN_LAUNCHER_SIZE 48

struct launcher
{
    WCHAR title[80];
    WCHAR shortcut[MAX_PATH];
    WCHAR target[MAX_PATH];
    HWND button;
    HWND running;
    HICON icon;
    BOOL active;
};

static struct launcher launchers[MAX_LAUNCHERS];
static UINT launcher_count, launch_mode;
static HWND launcher_owner;
static COLORREF launcher_background;

static BOOL shell_desktop_update_enabled(void)
{
    char value[8];
    DWORD len = GetEnvironmentVariableA("WATER_SHELL_DESKTOP_UPDATE",
                                         value, sizeof(value));
    return len == 1 && value[0] == '1';
}

static UINT select_launch_mode(void)
{
    struct water_win16_profile profile;
    char line[32], legacy[32];
    DWORD len;
    BOOL explicit_profile = FALSE;

    len = GetEnvironmentVariableA(WATER_WIN16_LINE_ENV, line, sizeof(line));
    if (len && len < sizeof(line)) explicit_profile = TRUE;
    len = GetEnvironmentVariableA(WATER_WIN16_PERSONALITY_ENV,
                                  legacy, sizeof(legacy));
    if (len && len < sizeof(legacy)) explicit_profile = TRUE;
    if (!explicit_profile) return 0;

    water_win16_read_profile(&profile);
    if (profile.line == WATER_WIN16_LINE_NT_FAMILY)
    {
        /* NT 4.0 requires IE4 Desktop Update. NT 5.0 through Vista have
         * a traditional optional Quick Launch; NT 6.1+ has taskbar pins. */
        if (profile.major >= 7 || (profile.major == 6 && profile.minor >= 1))
            return 2;
        if (profile.major == 4) return shell_desktop_update_enabled() ? 1 : 0;
        return profile.major >= 5 ? 1 : 0;
    }
    if (profile.line == WATER_WIN16_LINE_WIN9X_FAMILY && profile.major == 4)
    {
        if (profile.minor >= 10) return 1; /* Windows 98 and Me */
        return shell_desktop_update_enabled() ? 1 : 0;
    }
    return 0;
}

static BOOL quicklaunch_preference(void)
{
    DWORD value, size = sizeof(value);
    struct water_win16_profile profile;

    if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                      L"ShowQuickLaunch", RRF_RT_REG_DWORD, NULL, &value, &size))
        return !!value;

    /* Distinguish availability from initial visibility. Windows 98's
     * enhanced shell showed Quick Launch; XP/Vista did not enable it
     * by default for every newly installed user. */
    water_win16_read_profile(&profile);
    return profile.line == WATER_WIN16_LINE_WIN9X_FAMILY &&
           profile.major == 4 && profile.minor >= 10;
}

UINT quicklaunch_mode(void)
{
    return launch_mode;
}

static BOOL get_shortcut_folder(WCHAR *folder, UINT mode)
{
    WCHAR appdata[MAX_PATH];
    const WCHAR *subpath = mode == 2 ?
        L"Microsoft\\Internet Explorer\\Quick Launch\\User Pinned\\TaskBar" :
        L"Microsoft\\Internet Explorer\\Quick Launch";

    if (!SHGetSpecialFolderPathW(NULL, appdata, CSIDL_APPDATA, FALSE))
        return FALSE;
    if (lstrlenW(appdata) + lstrlenW(subpath) + 2 >= MAX_PATH) return FALSE;
    lstrcpyW(folder, appdata);
    lstrcatW(folder, L"\\");
    lstrcatW(folder, subpath);
    return TRUE;
}

static void get_shortcut_target(struct launcher *launcher)
{
    IShellLinkW *link;
    IPersistFile *persist = NULL;

    launcher->target[0] = 0;
    if (lstrcmpiW(PathFindExtensionW(launcher->shortcut), L".lnk")) return;
    if (FAILED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                &IID_IShellLinkW, (void **)&link))) return;

    if (SUCCEEDED(IShellLinkW_QueryInterface(link, &IID_IPersistFile,
                                            (void **)&persist)) && persist &&
        SUCCEEDED(IPersistFile_Load(persist, launcher->shortcut, STGM_READ)))
    {
        IShellLinkW_GetPath(link, launcher->target, ARRAY_SIZE(launcher->target),
                           NULL, SLGP_RAWPATH);
    }
    if (persist) IPersistFile_Release(persist);
    IShellLinkW_Release(link);
}

static void add_launcher(const WCHAR *name, const WCHAR *path)
{
    struct launcher *launcher;
    UINT i, index = 0;
    int cmp;

    if (launcher_count == MAX_LAUNCHERS) return;
    while (index < launcher_count)
    {
        cmp = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                             launchers[index].title, -1, name, -1);
        if (cmp == CSTR_EQUAL) return;
        if (cmp == CSTR_GREATER_THAN) break;
        index++;
    }
    for (i = launcher_count; i > index; --i) launchers[i] = launchers[i-1];
    launcher = &launchers[index];
    ZeroMemory(launcher, sizeof(*launcher));
    lstrcpynW(launcher->title, name, ARRAY_SIZE(launcher->title));
    lstrcpynW(launcher->shortcut, path, ARRAY_SIZE(launcher->shortcut));
    get_shortcut_target(launcher);
    launcher_count++;
}

static void load_launchers(void)
{
    WCHAR directory[MAX_PATH], search[MAX_PATH], path[MAX_PATH], title[MAX_PATH];
    WIN32_FIND_DATAW found;
    HANDLE handle;
    int length;

    launcher_count = 0;
    if (!launch_mode || !get_shortcut_folder(directory, launch_mode)) return;
    length = lstrlenW(directory);
    if (length + 3 >= MAX_PATH) return;
    lstrcpyW(search, directory);
    lstrcatW(search, L"\\*");
    handle = FindFirstFileW(search, &found);
    if (handle == INVALID_HANDLE_VALUE) return;
    do
    {
        const WCHAR *extension;
        int suffix;

        if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN |
                                     FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        extension = wcsrchr(found.cFileName, L'.');
        if (!extension || (lstrcmpiW(extension, L".lnk") &&
                           (launch_mode == 2 || lstrcmpiW(extension, L".url"))))
            continue;
        if (length + lstrlenW(found.cFileName) + 2 >= MAX_PATH) continue;

        lstrcpyW(path, directory);
        lstrcatW(path, L"\\");
        lstrcatW(path, found.cFileName);
        lstrcpynW(title, found.cFileName, ARRAY_SIZE(title));
        suffix = lstrlenW(title);
        if (suffix <= 4) continue;
        title[suffix - 4] = 0;
        add_launcher(title, path);
    } while (launcher_count < MAX_LAUNCHERS && FindNextFileW(handle, &found));
    FindClose(handle);
}

static struct launcher *find_launcher(HWND button)
{
    UINT i;
    for (i = 0; i < launcher_count; ++i)
        if (launchers[i].button == button) return &launchers[i];
    return NULL;
}

static void clear_launcher_buttons(void)
{
    UINT i;
    for (i = 0; i < launcher_count; ++i)
    {
        if (launchers[i].button) DestroyWindow(launchers[i].button);
        if (launchers[i].icon) DestroyIcon(launchers[i].icon);
    }
    ZeroMemory(launchers, sizeof(launchers));
    launcher_count = 0;
}

void quicklaunch_initialize(HWND owner, COLORREF background)
{
    UINT i;

    if (launcher_owner)
        clear_launcher_buttons();
    launcher_owner = owner;
    launcher_background = background;
    launch_mode = select_launch_mode();
    if (launch_mode == 1 && !quicklaunch_preference()) launch_mode = 0;
    if (!launch_mode) return;

    load_launchers();
    for (i = 0; i < launcher_count; i++)
    {
        SHFILEINFOW info = {0};
        struct launcher *l = &launchers[i];

        if (SHGetFileInfoW(l->shortcut, 0, &info, sizeof(info),
                           SHGFI_ICON | SHGFI_SMALLICON))
            l->icon = info.hIcon;
        l->button = CreateWindowW(WC_BUTTONW, l->title, WS_CHILD | BS_OWNERDRAW,
                                   0, 0, 0, 0, owner, NULL, NULL, NULL);
        if (!l->button)
            WARN("Could not create Quick Launch button for %s\n",
                 debugstr_w(l->shortcut));
    }
}

void quicklaunch_reset_running(void)
{
    UINT i;

    for (i = 0; i < launcher_count; ++i)
    {
        launchers[i].running = NULL;
        launchers[i].active = FALSE;
    }
}

/* A simple executable-path equivalence is conservative: unknown and
 * AppUserModelID-only pins never collapse a running window incorrectly.
 * Full AppUserModelID matching and multiwindow groups remain future work. */
BOOL quicklaunch_bind_window(HWND hwnd, const WCHAR *executable, BOOL active)
{
    UINT i;

    if (launch_mode != 2 || !executable || !executable[0]) return FALSE;
    for (i = 0; i < launcher_count; ++i)
    {
        struct launcher *l = &launchers[i];
        if (l->button && l->target[0] && !lstrcmpiW(l->target, executable) &&
            !l->running)
        {
            l->running = hwnd;
            l->active = active;
            return TRUE;
        }
    }
    return FALSE;
}

int quicklaunch_layout(int pos, int right, BOOL vertical,
                       int full_width, int full_height)
{
    UINT i;
    int length = launch_mode == 2 ? MODERN_LAUNCHER_SIZE : OLD_LAUNCHER_SIZE;

    for (i = 0; i < launcher_count; ++i)
    {
        HWND button = launchers[i].button;
        if (!button) continue;
        if (pos + length > right)
        {
            ShowWindow(button, SW_HIDE);
            continue;
        }
        SetWindowPos(button, NULL, vertical ? 0 : pos, vertical ? pos : 0,
                     vertical ? full_width : length, vertical ? length : full_height,
                     SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(button, NULL, FALSE);
        pos += length;
    }
    return pos;
}

BOOL quicklaunch_paint(const DRAWITEMSTRUCT *draw)
{
    struct launcher *l = find_launcher(draw->hwndItem);
    HBRUSH brush;
    RECT rect;
    HICON icon;

    if (!l) return FALSE;
    GetClientRect(draw->hwndItem, &rect);
    brush = CreateSolidBrush(launcher_background);
    if (brush)
    {
        FillRect(draw->hDC, &rect, brush);
        DeleteObject(brush);
    }
    if (launch_mode == 1)
        DrawEdge(draw->hDC, &rect, BDR_RAISEDINNER, BF_RIGHT);
    else if (l->running)
    {
        RECT line = rect;
        line.top = line.bottom - 3;
        brush = CreateSolidBrush(l->active ? RGB(125, 198, 246) : RGB(92, 143, 185));
        if (brush)
        {
            FillRect(draw->hDC, &line, brush);
            DeleteObject(brush);
        }
    }
    icon = l->icon ? l->icon : LoadIconW(NULL, IDI_APPLICATION);
    DrawIconEx(draw->hDC, (rect.right - (launch_mode == 2 ? 32 : 16)) / 2,
               (rect.bottom - (launch_mode == 2 ? 32 : 16)) / 2,
               icon, launch_mode == 2 ? 32 : 16, launch_mode == 2 ? 32 : 16,
               0, NULL, DI_NORMAL);
    return TRUE;
}

BOOL quicklaunch_click(HWND button)
{
    struct launcher *l = find_launcher(button);
    HINSTANCE result;

    if (!l) return FALSE;
    if (l->running && IsWindow(l->running))
    {
        if (IsIconic(l->running))
            SendMessageW(l->running, WM_SYSCOMMAND, SC_RESTORE, 0);
        else if (GetForegroundWindow() == l->running)
            SendMessageW(l->running, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        else SetForegroundWindow(l->running);
    }
    else
    {
        result = ShellExecuteW(launcher_owner, L"open", l->shortcut,
                               NULL, NULL, SW_SHOWNORMAL);
        if ((INT_PTR)result <= 32)
            WARN("Cannot open launcher shortcut %s (%Id)\n",
                 debugstr_w(l->shortcut), (INT_PTR)result);
    }
    return TRUE;
}
