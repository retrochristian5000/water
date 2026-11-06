/*
 * Windows 8 (NT 6.2) / Windows 8.1 (NT 6.3) Start screen.
 * Desktop shortcut tiles only: no WinRT, live tiles, Store, or charms.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string.h>
#include <wchar.h>
#include "wine/win16_profile.h"
#include "wine/debug.h"
#include "explorer_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(explorer);

#define MAX_TILES 128
#define TILE_W 180
#define TILE_H 92
#define TILE_STEP_X 194
#define TILE_STEP_Y 108
#define TILE_LEFT 70
#define TILE_TOP 155
#define HOT_SIZE 40

struct shortcut_tile { WCHAR label[96]; WCHAR file[MAX_PATH]; };
static struct shortcut_tile tiles[MAX_TILES];
static UINT tile_count, offset_columns;
static WCHAR query[64];
static HWND screen, corner, corner_owner, return_to;
static BOOL corner_hover;
static HFONT heading_font, tile_font;

UINT start_screen_version(void)
{
    char line[32], version[32];
    WORD major, minor;
    DWORD build, n;

    /* Require an explicit NT line and version, never the Win9x fallback. */
    n = GetEnvironmentVariableA(WATER_WIN16_LINE_ENV, line, sizeof(line));
    if (!n || n >= sizeof(line) || strcmp(line, WATER_WIN16_LINE_NT)) return 0;
    n = GetEnvironmentVariableA(WATER_WIN16_VERSION_ENV, version, sizeof(version));
    if (!n || n >= sizeof(version) ||
        !water_win16_parse_version(version, &major, &minor, &build)) return 0;
    return major == 6 && (minor == 2 || minor == 3) ? minor : 0;
}

static void insert_shortcut(const WCHAR *name, const WCHAR *path)
{
    UINT index = 1, i;
    int order;

    if (tile_count >= MAX_TILES || !name[0]) return;
    while (index < tile_count)
    {
        order = CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                tiles[index].label, -1, name, -1);
        if (order == CSTR_EQUAL) return;
        if (order == CSTR_GREATER_THAN) break;
        index++;
    }
    for (i = tile_count; i > index; i--) tiles[i] = tiles[i - 1];
    lstrcpynW(tiles[index].label, name, ARRAY_SIZE(tiles[index].label));
    lstrcpynW(tiles[index].file, path, ARRAY_SIZE(tiles[index].file));
    tile_count++;
}

static void scan_shortcut_folder(const WCHAR *folder, UINT depth)
{
    WIN32_FIND_DATAW found;
    WCHAR pattern[MAX_PATH], path[MAX_PATH], name[MAX_PATH];
    HANDLE search;
    int length, label_length;

    if (depth > 4 || tile_count == MAX_TILES) return;
    length = lstrlenW(folder);
    if (length + 3 >= MAX_PATH) return;
    lstrcpyW(pattern, folder);
    lstrcatW(pattern, L"\\*");
    search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE) return;
    do
    {
        const WCHAR *ext;

        if (found.cFileName[0] == L'.' ||
            found.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM |
                                      FILE_ATTRIBUTE_REPARSE_POINT)) continue;
        if (length + lstrlenW(found.cFileName) + 2 >= MAX_PATH) continue;
        lstrcpyW(path, folder);
        lstrcatW(path, L"\\");
        lstrcatW(path, found.cFileName);
        if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            scan_shortcut_folder(path, depth + 1);
            continue;
        }
        ext = wcsrchr(found.cFileName, L'.');
        if (!ext || (lstrcmpiW(ext, L".lnk") && lstrcmpiW(ext, L".url"))) continue;
        lstrcpynW(name, found.cFileName, ARRAY_SIZE(name));
        label_length = lstrlenW(name);
        if (label_length <= 4) continue;
        name[label_length - 4] = 0;
        insert_shortcut(name, path);
    } while (tile_count < MAX_TILES && FindNextFileW(search, &found));
    FindClose(search);
}

static void load_tiles(void)
{
    WCHAR folder[MAX_PATH];

    tile_count = 1;
    lstrcpyW(tiles[0].label, L"Desktop");
    tiles[0].file[0] = 0;
    if (SHGetSpecialFolderPathW(NULL, folder, CSIDL_PROGRAMS, FALSE))
        scan_shortcut_folder(folder, 0);
    if (SHGetSpecialFolderPathW(NULL, folder, CSIDL_COMMON_PROGRAMS, FALSE))
        scan_shortcut_folder(folder, 0);
}

static int tile_number(UINT ordinal)
{
    UINT i;
    for (i = 0; i < tile_count; i++)
        if (!query[0] || StrStrIW(tiles[i].label, query))
        {
            if (!ordinal) return i;
            ordinal--;
        }
    return -1;
}

static UINT tile_total(void)
{
    UINT i, count = 0;
    for (i = 0; i < tile_count; i++)
        if (!query[0] || StrStrIW(tiles[i].label, query)) count++;
    return count;
}

static UINT tile_rows(void)
{
    RECT rc = {0};
    if (screen) GetClientRect(screen, &rc);
    return max(1, (rc.bottom - TILE_TOP - 44) / TILE_STEP_Y);
}

static void limit_scroll(void)
{
    RECT rc = {0};
    UINT visible, total, rows = tile_rows();
    if (screen) GetClientRect(screen, &rc);
    visible = max(1, (rc.right - TILE_LEFT) / TILE_STEP_X);
    total = (tile_total() + rows - 1) / rows;
    if (offset_columns > (total > visible ? total - visible : 0))
        offset_columns = total > visible ? total - visible : 0;
}

static void close_screen(void)
{
    if (!screen) return;
    ShowWindow(screen, SW_HIDE);
    if (return_to && IsWindow(return_to)) SetForegroundWindow(return_to);
}

static void open_tile(int index)
{
    HINSTANCE result;
    if (index < 0 || (UINT)index >= tile_count) return;
    if (!index) { close_screen(); return; }
    result = ShellExecuteW(screen, L"open", tiles[index].file, NULL, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result > 32) close_screen();
    else WARN("Start shortcut failed: %s\n", debugstr_w(tiles[index].file));
}

static void paint_screen(HDC dc)
{
    static const COLORREF colors[] = { RGB(0,112,191), RGB(97,46,154),
                                      RGB(0,143,120), RGB(181,48,85),
                                      RGB(178,94,0), RGB(0,120,158) };
    RECT rc, cell;
    HBRUSH brush;
    HFONT saved;
    UINT n, rows = tile_rows(), count = tile_total();
    WCHAR caption[96] = L"Desktop shortcuts";

    GetClientRect(screen, &rc);
    brush = CreateSolidBrush(RGB(36,14,77));
    FillRect(dc, &rc, brush);
    DeleteObject(brush);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255,255,255));
    saved = SelectObject(dc, heading_font);
    SetRect(&cell, TILE_LEFT, 40, rc.right - 30, 110);
    DrawTextW(dc, L"Start", -1, &cell, DT_LEFT | DT_SINGLELINE);

    SelectObject(dc, tile_font);
    if (query[0])
    {
        lstrcpyW(caption, L"Search: ");
        lstrcatW(caption, query);
    }
    SetRect(&cell, TILE_LEFT, 111, rc.right - 30, 145);
    DrawTextW(dc, caption, -1, &cell, DT_LEFT | DT_SINGLELINE);

    /* The signed-in account's chosen picture and display name appear at
     * the upper right, independent of shortcut tile ownership. */
    SetRect(&cell, max(TILE_LEFT + 250, rc.right - 300), 24, rc.right - 40, 92);
    paint_shell_account(dc, &cell, TRUE);

    for (n = offset_columns * rows; n < count; n++)
    {
        int index = tile_number(n);
        UINT col = n / rows - offset_columns, row = n % rows;
        SHFILEINFOW info;
        HICON image = NULL;

        cell.left = TILE_LEFT + col * TILE_STEP_X;
        cell.top = TILE_TOP + row * TILE_STEP_Y;
        cell.right = cell.left + TILE_W;
        cell.bottom = cell.top + TILE_H;
        if (cell.left >= rc.right) break;
        if (cell.bottom > rc.bottom || index < 0) continue;
        brush = CreateSolidBrush(colors[index % ARRAY_SIZE(colors)]);
        FillRect(dc, &cell, brush);
        DeleteObject(brush);

        if (!index) image = LoadIconW(NULL, IDI_APPLICATION);
        else if (SHGetFileInfoW(tiles[index].file, 0, &info, sizeof(info),
                                SHGFI_ICON | SHGFI_LARGEICON)) image = info.hIcon;
        if (image) DrawIconEx(dc, cell.left + 14, cell.top + 12, image, 32, 32, 0, 0, DI_NORMAL);
        if (index && image) DestroyIcon(image);

        cell.left += 12;
        cell.right -= 8;
        cell.top = cell.bottom - 36;
        cell.bottom -= 7;
        DrawTextW(dc, tiles[index].label, -1, &cell,
                  DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    SelectObject(dc, saved);
}

static LRESULT CALLBACK screen_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    switch (message)
    {
    case WM_CREATE:
        heading_font = CreateFontW(-46, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
                                   DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI Light");
        tile_font = CreateFontW(-19, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                                DEFAULT_CHARSET, 0, 0, DEFAULT_QUALITY, 0, L"Segoe UI");
        return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        paint_screen(dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_LBUTTONUP:
    {
        POINT point = {(short)LOWORD(lp), (short)HIWORD(lp)};
        RECT rc;
        int x = point.x - TILE_LEFT, y = point.y - TILE_TOP;

        GetClientRect(hwnd, &rc);
        if (point.y >= 24 && point.y < 92 &&
            point.x >= max(TILE_LEFT + 250, rc.right - 300) && point.x < rc.right - 40)
        {
            edit_shell_account(hwnd);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        UINT row, col;
        if (x < 0 || y < 0 || x % TILE_STEP_X >= TILE_W ||
            y % TILE_STEP_Y >= TILE_H) return 0;
        row = y / TILE_STEP_Y;
        col = x / TILE_STEP_X;
        if (row < tile_rows()) open_tile(tile_number((col + offset_columns) * tile_rows() + row));
        return 0;
    }
    case WM_MOUSEWHEEL:
        if ((short)HIWORD(wp) > 0 && offset_columns) offset_columns--;
        else if ((short)HIWORD(wp) < 0) offset_columns++;
        limit_scroll();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_CHAR:
        if (wp >= 32 && wp < 127 && lstrlenW(query) < ARRAY_SIZE(query) - 1)
        {
            WCHAR character[2] = {(WCHAR)wp, 0};
            lstrcatW(query, character);
            offset_columns = 0;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE || wp == VK_LWIN || wp == VK_RWIN)
        {
            close_screen();
            return 0;
        }
        if (wp == VK_BACK && query[0])
        {
            query[lstrlenW(query) - 1] = 0;
            offset_columns = 0;
        }
        else if (wp == VK_RIGHT) offset_columns++;
        else if (wp == VK_LEFT && offset_columns) offset_columns--;
        limit_scroll();
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    case WM_SIZE:
        if (screen == hwnd) limit_scroll();
        return 0;
    case WM_DISPLAYCHANGE:
        SetWindowPos(hwnd, NULL, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                     GetSystemMetrics(SM_CYSCREEN), SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    case WM_DESTROY:
        DeleteObject(heading_font);
        DeleteObject(tile_font);
        screen = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

void show_start_screen(HWND owner)
{
    static const WCHAR name[] = L"WaterStartScreen";
    WNDCLASSW wc = {0};

    if (!start_screen_version()) return;
    if (screen && IsWindowVisible(screen)) { close_screen(); return; }
    if (!screen)
    {
        wc.hInstance = GetModuleHandleW(NULL);
        wc.lpfnWndProc = screen_proc;
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.lpszClassName = name;
        RegisterClassW(&wc);
        screen = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, name, L"Start",
                                 WS_POPUP, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                                 GetSystemMetrics(SM_CYSCREEN), owner, NULL, wc.hInstance, NULL);
        if (!screen) { WARN("Could not create Start screen\n"); return; }
    }
    return_to = GetForegroundWindow();
    query[0] = 0;
    offset_columns = 0;
    load_tiles();
    SetWindowPos(screen, HWND_TOPMOST, 0, 0, GetSystemMetrics(SM_CXSCREEN),
                 GetSystemMetrics(SM_CYSCREEN), SWP_SHOWWINDOW);
    SetForegroundWindow(screen);
    SetFocus(screen);
    InvalidateRect(screen, NULL, FALSE);
}

static void move_corner(HWND hwnd)
{
    SetWindowPos(hwnd, HWND_TOPMOST, 0, GetSystemMetrics(SM_CYSCREEN) - HOT_SIZE,
                 HOT_SIZE, HOT_SIZE, SWP_SHOWWINDOW | SWP_NOACTIVATE);
}

static LRESULT CALLBACK corner_proc(HWND hwnd, UINT message, WPARAM wp, LPARAM lp)
{
    switch (message)
    {
    case WM_MOUSEMOVE:
        if (!corner_hover)
        {
            TRACKMOUSEEVENT event = {sizeof(event), TME_LEAVE, hwnd, 0};
            corner_hover = TRUE;
            TrackMouseEvent(&event);
            SetLayeredWindowAttributes(hwnd, 0, 235, LWA_ALPHA);
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    case WM_MOUSELEAVE:
        corner_hover = FALSE;
        SetLayeredWindowAttributes(hwnd, 0, 1, LWA_ALPHA);
        return 0;
    case WM_LBUTTONUP:
        show_start_screen(corner_owner);
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC dc = BeginPaint(hwnd, &ps);
        HBRUSH brush = CreateSolidBrush(RGB(45,30,94));
        GetClientRect(hwnd, &rc);
        FillRect(dc, &rc, brush);
        DeleteObject(brush);
        SetTextColor(dc, RGB(255,255,255));
        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, L"Start", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DISPLAYCHANGE: move_corner(hwnd); return 0;
    case WM_DESTROY: corner = NULL; return 0;
    }
    return DefWindowProcW(hwnd, message, wp, lp);
}

void initialize_start_corner(HWND owner)
{
    static const WCHAR name[] = L"WaterStartCorner";
    WNDCLASSW wc = {0};

    if (start_screen_version() != 2 || corner) return;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.lpfnWndProc = corner_proc;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = name;
    RegisterClassW(&wc);
    corner_owner = owner;
    corner = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE |
                             WS_EX_TOOLWINDOW, name, L"", WS_POPUP,
                             0, 0, HOT_SIZE, HOT_SIZE, NULL, NULL, wc.hInstance, NULL);
    if (!corner) { WARN("Could not create Start hot corner\n"); return; }
    SetLayeredWindowAttributes(corner, 0, 1, LWA_ALPHA);
    move_corner(corner);
}
