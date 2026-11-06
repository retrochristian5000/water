/*
 * Explorer.exe tests
 *
 * Copyright 2022 Zhiyi Zhang for CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <windows.h>
#include <shellapi.h>
#include <string.h>
#include "wine/test.h"

static void test_taskbar(void)
{
    RECT taskbar_rect, expected_rect, primary_rect, work_rect;
    HWND hwnd;
    BOOL ret;

    hwnd = FindWindowA("Shell_TrayWnd", NULL);
    if (!hwnd)
    {
        skip("Failed to find the taskbar window.\n");
        return;
    }

    ret = GetWindowRect(hwnd, &taskbar_rect);
    ok(ret, "GetWindowRect failed, error %ld.\n", GetLastError());

    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work_rect, 0);
    SetRect(&primary_rect, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    SubtractRect(&expected_rect, &primary_rect, &work_rect);

    /* In standalone mode, the systray window is floating */
    todo_wine_if(!(GetWindowLongW(hwnd, GWL_STYLE) & WS_POPUP))
    ok(EqualRect(&taskbar_rect, &expected_rect), "Expected %s, got %s.\n",
       wine_dbgstr_rect(&expected_rect), wine_dbgstr_rect(&taskbar_rect));
}

static void test_taskbar_appbar(void)
{
    APPBARDATA abd = {0};
    RECT rect;
    HWND hwnd, clock;
    UINT_PTR state;
    BOOL ret, water_shell;
    HMODULE ntdll;
    WCHAR time[64];

    ntdll = GetModuleHandleA("ntdll.dll");
    water_shell = ntdll && !!GetProcAddress(ntdll, "wine_get_version");
    hwnd = FindWindowA("Shell_TrayWnd", NULL);
    if (!hwnd)
    {
        skip("No taskbar or notification window found.\n");
        return;
    }

    abd.cbSize = sizeof(abd);
    if (!(GetWindowLongW(hwnd, GWL_STYLE) & WS_POPUP))
    {
        if (water_shell)
            ok(!SHAppBarMessage(ABM_GETTASKBARPOS, &abd),
               "Standalone notification window was reported as a docked taskbar.\n");
        skip("Standalone notification area; no docked taskbar.\n");
        return;
    }
    ret = SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    ok(ret, "ABM_GETTASKBARPOS failed.\n");
    if (ret)
    {
        GetWindowRect(hwnd, &rect);
        ok(EqualRect(&abd.rc, &rect), "Expected %s, got %s.\n",
           wine_dbgstr_rect(&rect), wine_dbgstr_rect(&abd.rc));
        if (water_shell)
        {
            DWORD edge, size = sizeof(edge);
            LONG status;

            ok(abd.uEdge <= ABE_BOTTOM, "Invalid taskbar edge %u.\n", abd.uEdge);
            status = RegGetValueW( HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                                   L"Edge", RRF_RT_REG_DWORD, NULL, &edge, &size );
            if (!status && edge <= ABE_BOTTOM)
                ok(abd.uEdge == edge, "Saved edge %lu, reported edge %u.\n", edge, abd.uEdge);
        }
    }

    /* Water's taskbar does not yet implement autohide or the configurable clock. */
    if (!water_shell) return;
    state = SHAppBarMessage(ABM_GETSTATE, &abd);
    ok(!(state & ABS_AUTOHIDE), "Autohide was advertised without an implementation.\n");

    clock = FindWindowExW(hwnd, NULL, L"Static", NULL);
    ok(!!clock, "Classic taskbar clock was not created.\n");
    if (clock)
    {
        ret = GetWindowTextW(clock, time, ARRAY_SIZE(time));
        ok(ret, "Classic taskbar clock has no displayed time.\n");
    }
}

static void test_win8_start(void)
{
    char line[32], version[32];
    DWORD len;
    HWND corner, screen;

    len = GetEnvironmentVariableA("WATER_VDM_LINE", line, sizeof(line));
    if (!len || len >= sizeof(line) || strcmp(line, "nt")) return;
    len = GetEnvironmentVariableA("WATER_VDM_VERSION", version, sizeof(version));
    if (!len || len >= sizeof(version)) return;

    if (!strcmp(version, "6.2") || !strncmp(version, "6.2.", 4))
    {
        corner = FindWindowW(L"WaterStartCorner", NULL);
        ok(!!corner, "Windows 8 needs a lower-left Start hot corner.\n");
        if (!corner) return;
        screen = FindWindowW(L"WaterStartScreen", NULL);
        ok(!!screen && IsWindowVisible(screen), "Windows 8 RTM did not start on Start screen.\n");
        if (screen) SendMessageW(screen, WM_KEYDOWN, VK_ESCAPE, 0);
        SendMessageW(corner, WM_LBUTTONUP, 0, 0);
        screen = FindWindowW(L"WaterStartScreen", NULL);
        ok(!!screen && IsWindowVisible(screen), "Hot corner failed to reopen Start screen.\n");
        if (screen)
        {
            SendMessageW(screen, WM_KEYDOWN, VK_ESCAPE, 0);
            ok(!IsWindowVisible(screen), "Escape failed to close Start screen.\n");
        }
    }
    else if (!strcmp(version, "6.3") || !strncmp(version, "6.3.", 4))
        ok(!FindWindowW(L"WaterStartCorner", NULL),
           "Windows 8.1 must use its restored taskbar Start button.\n");
}


/* Verify that theme selection never bleeds between guest OS personalities.
 * Classic shells keep a class background brush; XP/Vista paint their own. */
static void test_taskbar_visual(void)
{
    char line[32], version[32];
    DWORD len;
    HWND hwnd;
    BOOL expected_themed = FALSE;
    HBRUSH background;
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");

    if (!ntdll || !GetProcAddress(ntdll, "wine_get_version")) return;

    hwnd = FindWindowA("Shell_TrayWnd", NULL);
    if (!hwnd || !(GetWindowLongW(hwnd, GWL_STYLE) & WS_POPUP)) return;

    len = GetEnvironmentVariableA("WATER_VDM_LINE", line, sizeof(line));
    if (len && len < sizeof(line) && !strcmp(line, "nt"))
    {
        len = GetEnvironmentVariableA("WATER_VDM_VERSION", version, sizeof(version));
        if (len && len < sizeof(version))
            expected_themed = !strcmp(version, "5.1") || !strncmp(version, "5.1.", 4) ||
                              !strcmp(version, "6.0") || !strncmp(version, "6.0.", 4);
    }
    background = (HBRUSH)GetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND);
    ok(expected_themed ? !background : !!background,
       "Unexpected taskbar class background for selected Windows version.\n");
}

/* Verify profile UI is never required for classic Win9x guests. The
 * actual account identity remains managed by Windows, not Explorer. */
static void test_account_identity(void)
{
    WCHAR name[256];
    DWORD count = ARRAY_SIZE(name);

    ok(GetUserNameW(name, &count) && name[0], "No current user identity.\n");
}

static void test_input_language_indicator(void)
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL), lang, clock;
    RECT lang_rect, clock_rect, overlap;
    int count;

    if (!tray || !(GetWindowLongPtrW(tray, GWL_STYLE) & WS_POPUP)) return;
    lang = FindWindowExW(tray, NULL, L"WaterInputLanguage", NULL);
    if (!lang) { skip("Input language indicator is not enabled in this shell.\n"); return; }
    count = GetKeyboardLayoutList(0, NULL);
    if (count <= 1)
    {
        ok(!IsWindowVisible(lang), "Input language indicator visible with one layout.\n");
        return;
    }
    if (!IsWindowVisible(lang))
    {
        skip("Input language indicator is hidden by user preferences.\n");
        return;
    }
    clock = FindWindowExW(tray, NULL, L"Static", NULL);
    ok(!!GetWindowRect(lang, &lang_rect), "Failed to read indicator geometry.\n");
    if (clock && GetWindowRect(clock, &clock_rect))
        ok(!IntersectRect(&overlap, &lang_rect, &clock_rect),
           "Language indicator overlaps taskbar clock.\n");
}


/* The ShowClock value is WATER-owned, independent of version-specific
 * StuckRects binary formats. GPO HideClock overrides a user's preference. */
static BOOL taskbar_policy_flag(HKEY root, const WCHAR *name)
{
    static const WCHAR key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    DWORD value, size = sizeof(value);
    return !RegGetValueW(root, key, name, RRF_RT_REG_DWORD, NULL, &value, &size) && !!value;
}

static void test_taskbar_clock_registry(void)
{
    static const WCHAR key[] = L"Software\\Wine\\Explorer\\Taskbar";
    DWORD value, size = sizeof(value);
    BOOL expected = TRUE;
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    HWND clock;

    if (!tray || !(GetWindowLongPtrW(tray, GWL_STYLE) & WS_POPUP)) return;
    if (!RegGetValueW(HKEY_CURRENT_USER, key, L"ShowClock",
                      RRF_RT_REG_DWORD, NULL, &value, &size))
        expected = !!value;
    if (taskbar_policy_flag(HKEY_CURRENT_USER, L"HideClock") ||
        taskbar_policy_flag(HKEY_LOCAL_MACHINE, L"HideClock"))
        expected = FALSE;

    clock = FindWindowExW(tray, NULL, L"Static", NULL);
    ok(!!clock, "Clock window should exist even when configured hidden.\n");
    if (clock)
        ok(!!IsWindowVisible(clock) == expected,
           "Taskbar clock visibility does not match registry/policy.\n");
}


/* Quick Launch must not fabricate shortcuts: all launcher controls correspond
 * to installed per-user .lnk/.url files, and old shell versions are gated. */
static void test_quicklaunch_boundaries(void)
{
    HWND tray = FindWindowW(L"Shell_TrayWnd", NULL);
    char line[32] = {0}, version[32] = {0}, update[8] = {0};
    DWORD n;
    HWND button, clock;
    RECT button_rect, clock_rect, overlap;
    UINT marked = 0;

    if (!tray || !(GetWindowLongPtrW(tray, GWL_STYLE) & WS_POPUP)) return;
    GetEnvironmentVariableA("WATER_VDM_LINE", line, sizeof(line));
    GetEnvironmentVariableA("WATER_VDM_VERSION", version, sizeof(version));
    n = GetEnvironmentVariableA("WATER_SHELL_DESKTOP_UPDATE", update, sizeof(update));

    /* Every visible launcher must have an independent slot and must not
     * collide with the notification-area clock. */
    clock = FindWindowExW(tray, NULL, L"Static", NULL);
    for (button = FindWindowExW(tray, NULL, L"Button", NULL); button;
         button = FindWindowExW(tray, button, L"Button", NULL))
    {
        if (!GetPropW(button, L"WaterQuickLaunch") || !IsWindowVisible(button)) continue;
        marked++;
        if (clock && IsWindowVisible(clock) && GetWindowRect(button, &button_rect) &&
            GetWindowRect(clock, &clock_rect))
            ok(!IntersectRect(&overlap, &button_rect, &clock_rect),
               "Quick Launch button overlaps taskbar clock.\n");
    }

    /* Original Windows 95 and NT 4 have no Quick Launch unless Desktop
     * Update has been explicitly selected. */
    if (( (!strcmp(line, "win9x") && !strcmp(version, "4.0")) ||
          (!strcmp(line, "nt") && !strcmp(version, "4.0")) ) &&
        (n != 1 || update[0] != '1'))
    {
        ok(!marked, "Unupdated Windows 95/NT4 exposed a Quick Launch button.\n");
    }
}


/* Text transport regression only. Glyph coverage is tested by shelltext.c
 * against installed fonts; this test must not assume proprietary fonts exist. */
static void test_cjk_window_title(void)
{
    static const WCHAR title[] = L"中文 繁體 日本語 漢字";
    WCHAR result[ARRAY_SIZE(title)];
    HWND window;

    window = CreateWindowExW(0, L"STATIC", title, WS_POPUP,
                             0, 0, 100, 30, NULL, NULL, NULL, NULL);
    if (!window)
    {
        skip("Cannot construct a Unicode CJK test window.\\n");
        return;
    }
    memset(result, 0, sizeof(result));
    ok(GetWindowTextW(window, result, ARRAY_SIZE(result)) ==
       ARRAY_SIZE(title) - 1, "CJK window title length changed.\\n");
    ok(!lstrcmpW(result, title), "Chinese/Japanese CJK title was altered.\\n");
    DestroyWindow(window);
}


/* In Win9x, CP_ACP is never UTF-8 and DBCS lead-byte detection must use
 * the same table as the A/W conversion routines. The current keyboard HKL
 * does not change the machine's ANSI system code page. */
static BOOL test_win9x_selected(void)
{
    char line[32], legacy[32];
    DWORD len;

    len = GetEnvironmentVariableA("WATER_VDM_LINE", line, sizeof(line));
    if (len) return len < sizeof(line) && !strcmp(line, "win9x");

    len = GetEnvironmentVariableA("WATER_VDM_PERSONALITY", legacy, sizeof(legacy));
    return len && len < sizeof(legacy) &&
           (!strcmp(legacy, "win95-osr2") || !strcmp(legacy, "winme"));
}

static void test_legacy_nls(void)
{
    static const struct
    {
        UINT codepage;
        BYTE bytes[2];
        WCHAR decoded;
    } examples[] =
    {
        { 932, {0x82, 0xa0}, 0x3042 }, /* Shift-JIS Japanese あ */
        { 936, {0xd6, 0xd0}, 0x4e2d }, /* GBK Chinese 中 */
        { 950, {0xa4, 0xa4}, 0x4e2d }  /* Big5 Chinese 中 */
    };
    UINT i;

    if (test_win9x_selected())
    {
        ok(GetACP() != CP_UTF8, "Win9x guest incorrectly exposes UTF-8 as ACP.\n");
        ok(IsDBCSLeadByte(0x82) == IsDBCSLeadByteEx(CP_ACP, 0x82),
           "Win9x ACP DBCS metadata differs between NLS entrypoints.\n");
    }

    for (i = 0; i < ARRAY_SIZE(examples); i++)
    {
        CPINFO info;
        WCHAR decoded = 0;
        char encoded[4] = {0};
        BOOL substituted = FALSE;
        int ret;

        if (!GetCPInfo(examples[i].codepage, &info))
        {
            skip("Code page %u is not installed.\n", examples[i].codepage);
            continue;
        }
        ok(info.MaxCharSize >= 2, "Code page %u lost its DBCS metadata.\n", examples[i].codepage);
        ok(IsDBCSLeadByteEx(examples[i].codepage, examples[i].bytes[0]),
           "Code page %u missing expected lead byte.\n", examples[i].codepage);
        ret = MultiByteToWideChar(examples[i].codepage, 0,
                                  (const char *)examples[i].bytes, 2, &decoded, 1);
        ok(ret == 1 && decoded == examples[i].decoded,
           "Code page %u did not decode the historical sample.\n", examples[i].codepage);
        ret = WideCharToMultiByte(examples[i].codepage, WC_NO_BEST_FIT_CHARS,
                                  &examples[i].decoded, 1, encoded, sizeof(encoded),
                                  NULL, &substituted);
        ok(ret == 2 && !substituted &&
           !memcmp(encoded, examples[i].bytes, 2),
           "Code page %u failed its exact DBCS round trip.\n", examples[i].codepage);
    }
}

START_TEST(explorer)
{
    test_taskbar();
    test_taskbar_appbar();
    test_taskbar_visual();
    test_taskbar_clock_registry();
    test_input_language_indicator();
    test_quicklaunch_boundaries();
    test_cjk_window_title();
    test_legacy_nls();
    test_win8_start();
    test_account_identity();
}
