/*
 * Taskbar input-language indicator.
 *
 * The shell displays the current foreground thread's input locale and
 * requests layout changes from the target application. Do not call
 * ActivateKeyboardLayout() here: it would only change Explorer's thread.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <windows.h>
#include <shellapi.h>
#include <wchar.h>

#include "wine/debug.h"
#include "explorer_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(explorer);

#define INPUTLANG_TIMER 1
#define INPUTLANG_MAX 64
#define INPUTLANG_WIDTH 38
#define INPUTLANG_HEIGHT 25
#define INPUTLANG_MENU_FIRST 7000

static HWND indicator, tray_owner, focused_target;
static HKL available[INPUTLANG_MAX], selected;
static UINT available_count;
static WCHAR indicator_label[4];
static COLORREF background_color, text_color;

static BOOL inputlang_enabled(void)
{
    HKEY key;
    DWORD enabled = 1, kind = 0, size = sizeof(enabled);
    LONG result;

    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Wine\\Explorer\\Taskbar", 0,
                      KEY_QUERY_VALUE, &key))
        return TRUE;
    result = RegQueryValueExW(key, L"ShowInputLanguage", NULL, &kind,
                              (BYTE *)&enabled, &size);
    RegCloseKey(key);
    return result || kind != REG_DWORD || size != sizeof(enabled) || enabled != 0;
}

static void inputlang_name(HKL layout, WCHAR *name, UINT length)
{
    LANGID language = LOWORD((ULONG_PTR)layout);
    LCID locale = MAKELCID(language, SORT_DEFAULT);
    if (shell_text_is_win9x())
    {
        char code[16];

        if (!GetLocaleInfoA(locale, LOCALE_SISO639LANGNAME, code, sizeof(code)))
            lstrcpyA(code, "??");
        CharUpperBuffA(code, lstrlenA(code));
        if (!MultiByteToWideChar(CP_ACP, 0, code, -1, name, length))
            lstrcpynW(name, L"??", length);
        return;
    }
    if (!GetLocaleInfoW(locale, LOCALE_SISO639LANGNAME, name, length) || !name[0])
        lstrcpynW(name, L"??", length);
    CharUpperBuffW(name, lstrlenW(name));
}

/* Keep the last real application target while interacting with the shell.
 * Layout selection is per input thread, not a global explorer.exe setting. */
static HKL current_input_layout(void)
{
    HWND foreground = GetForegroundWindow();
    DWORD process = 0, thread;

    if (foreground && IsWindow(foreground))
    {
        thread = GetWindowThreadProcessId(foreground, &process);
        if (thread && process != GetCurrentProcessId())
            focused_target = GetAncestor(foreground, GA_ROOT);
    }

    if (focused_target && IsWindow(focused_target))
    {
        thread = GetWindowThreadProcessId(focused_target, NULL);
        if (thread) return GetKeyboardLayout(thread);
    }
    focused_target = NULL;
    return GetKeyboardLayout(0);
}

static void inputlang_refresh(void)
{
    HKL current;
    WCHAR label[ARRAY_SIZE(indicator_label)] = {0};
    UINT new_count = 0;
    BOOL should_show, was_visible;

    if (!indicator) return;
    should_show = inputlang_enabled() && GetKeyboardLayoutList(0, NULL) > 1;
    if (should_show)
    {
        int count = GetKeyboardLayoutList(INPUTLANG_MAX, available);
        if (count > 1) new_count = count;
        else should_show = FALSE;
    }

    was_visible = IsWindowVisible(indicator);
    if (!should_show)
    {
        available_count = 0;
        selected = NULL;
        if (was_visible)
        {
            ShowWindow(indicator, SW_HIDE);
            PostMessageW(tray_owner, WM_WATER_INPUTLANG_LAYOUT, 0, 0);
        }
        return;
    }

    available_count = new_count;
    current = current_input_layout();
    inputlang_name(current, label, ARRAY_SIZE(label));
    if (current != selected || lstrcmpW(label, indicator_label))
    {
        selected = current;
        lstrcpynW(indicator_label, label, ARRAY_SIZE(indicator_label));
        InvalidateRect(indicator, NULL, FALSE);
    }

    if (!was_visible)
    {
        ShowWindow(indicator, SW_SHOWNOACTIVATE);
        PostMessageW(tray_owner, WM_WATER_INPUTLANG_LAYOUT, 0, 0);
    }
}

static void inputlang_menu(void)
{
    HMENU menu;
    UINT i, command;
    POINT point;
    HWND target;
    HKL choices[INPUTLANG_MAX];

    inputlang_refresh();
    target = focused_target;
    if (available_count < 2 || !GetCursorPos(&point)) return;
    if (!(menu = CreatePopupMenu())) return;

    /* Snapshot choices; a layout could be added/removed while the menu runs. */
    for (i = 0; i < available_count; i++)
    {
        WCHAR language[64], description[120];
        LANGID id = LOWORD((ULONG_PTR)available[i]);
        LCID locale = MAKELCID(id, SORT_DEFAULT);

        choices[i] = available[i];
        /* Win9x locale strings are code-page encoded, while NT may use
         * native Unicode Hanzi/kanji. Keep both paths independent. */
        if (shell_text_is_win9x())
        {
            char languageA[96], descriptionA[128];

            if (!GetLocaleInfoA(locale, LOCALE_SNATIVELANGNAME,
                                languageA, ARRAY_SIZE(languageA)))
                lstrcpyA(languageA, "Keyboard");
            wsprintfA(descriptionA, "%s (%04x:%04x)", languageA,
                      HIWORD((ULONG_PTR)choices[i]), id);
            AppendMenuA(menu, MF_STRING |
                        (choices[i] == selected ? MF_CHECKED : MF_UNCHECKED),
                        INPUTLANG_MENU_FIRST + i, descriptionA);
        }
        else
        {
            if (!GetLocaleInfoW(locale, LOCALE_SNATIVELANGNAME,
                                language, ARRAY_SIZE(language)))
                lstrcpyW(language, L"Keyboard");
            wsprintfW(description, L"%s (%04x:%04x)", language,
                      HIWORD((ULONG_PTR)choices[i]), id);
            AppendMenuW(menu, MF_STRING |
                        (choices[i] == selected ? MF_CHECKED : MF_UNCHECKED),
                        INPUTLANG_MENU_FIRST + i, description);
        }
    }

    command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                             point.x, point.y, 0, indicator, NULL);
    PostMessageW(indicator, WM_NULL, 0, 0);
    DestroyMenu(menu);

    if (command >= INPUTLANG_MENU_FIRST &&
        command < INPUTLANG_MENU_FIRST + available_count &&
        target && IsWindow(target))
    {
        HKL requested = choices[command - INPUTLANG_MENU_FIRST];

        /* Applications may accept/reject this message. Show the effective
         * layout on the next refresh, not an invented successful change. */
        if (requested != selected)
            PostMessageW(target, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)requested);
    }
}

static LRESULT CALLBACK inputlang_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_TIMER:
        if (wp == INPUTLANG_TIMER) inputlang_refresh();
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_CONTEXTMENU:
        inputlang_menu();
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect;
        HFONT old;
        HBRUSH brush = CreateSolidBrush(background_color);

        GetClientRect(hwnd, &rect);
        FillRect(dc, &rect, brush);
        DeleteObject(brush);
        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, text_color);
        old = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
        shell_draw_text(dc, indicator_label, &rect,
                        DT_CENTER | DT_VCENTER | DT_SINGLELINE,
                        LOWORD((ULONG_PTR)selected));
        SelectObject(dc, old);
        EndPaint(hwnd, &paint);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, INPUTLANG_TIMER);
        indicator = NULL;
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void inputlang_initialize(HWND owner, COLORREF background, COLORREF text)
{
    static const WCHAR name[] = L"WaterInputLanguage";
    WNDCLASSW wc = {0};

    if (indicator) return;
    tray_owner = owner;
    background_color = background;
    text_color = text;
    wc.lpfnWndProc = inputlang_proc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.lpszClassName = name;
    RegisterClassW(&wc);
    indicator = CreateWindowExW(WS_EX_NOACTIVATE, name, L"",
                  WS_CHILD | WS_CLIPSIBLINGS, 0, 0,
                  INPUTLANG_WIDTH, INPUTLANG_HEIGHT,
                  owner, NULL, wc.hInstance, NULL);
    if (!indicator)
    {
        WARN("Could not create taskbar input language indicator\n");
        return;
    }
    indicator_label[0] = 0;
    selected = NULL;
    inputlang_refresh();
    SetTimer(indicator, INPUTLANG_TIMER, 1000, NULL);
}

int inputlang_extent(BOOL vertical)
{
    if (!indicator || !IsWindowVisible(indicator)) return 0;
    return vertical ? INPUTLANG_HEIGHT : INPUTLANG_WIDTH;
}

void inputlang_layout(int width, int height, int clock_width, int clock_height,
                      int icon_width, int icon_height, UINT icon_count, UINT edge)
{
    int x, y, w = INPUTLANG_WIDTH, h = INPUTLANG_HEIGHT;
    BOOL vertical = edge == ABE_LEFT || edge == ABE_RIGHT;

    if (!indicator || !IsWindowVisible(indicator)) return;
    if (vertical)
    {
        x = (width - w) / 2;
        y = height - clock_height - icon_height * icon_count - h;
    }
    else
    {
        x = width - clock_width - icon_width * icon_count - w;
        y = (height - h) / 2;
    }
    SetWindowPos(indicator, NULL, max(0, x), max(0, y), w, h,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}
