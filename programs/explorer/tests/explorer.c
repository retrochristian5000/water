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
        SendMessageW(corner, WM_LBUTTONUP, 0, 0);
        screen = FindWindowW(L"WaterStartScreen", NULL);
        ok(!!screen && IsWindowVisible(screen), "Hot corner failed to open Start screen.\n");
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

START_TEST(explorer)
{
    test_taskbar();
    test_taskbar_appbar();
    test_win8_start();
}
