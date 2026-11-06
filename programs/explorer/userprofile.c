/*
 * Explorer user account presentation and per-user preferences.
 *
 * The signed-in security identity belongs to Windows. This module only
 * provides a presentation-only display name and installed BMP account
 * picture, scoped to HKCU. It never changes credentials or account SIDs.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <string.h>

#include "wine/debug.h"
#include "wine/win16_profile.h"
#include "explorer_private.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(explorer);

static const WCHAR account_key[] = L"Software\\Wine\\Explorer\\UserAccount";

/* An absent guest NT selector never inherits the default Win9x profile. */
static BOOL get_shell_version( WORD *major, WORD *minor )
{
    char line[32], version[32];
    DWORD n, build;

    n = GetEnvironmentVariableA( WATER_WIN16_LINE_ENV, line, sizeof(line) );
    if (!n || n >= sizeof(line) || strcmp(line, WATER_WIN16_LINE_NT)) return FALSE;
    n = GetEnvironmentVariableA( WATER_WIN16_VERSION_ENV, version, sizeof(version) );
    return n && n < sizeof(version) &&
           water_win16_parse_version( version, major, minor, &build );
}

BOOL shell_account_classic_header(void)
{
    WORD major, minor;
    HKEY key;
    DWORD policy = 0, type = 0, size = sizeof(policy);

    if (!get_shell_version(&major, &minor)) return FALSE;
    if (!((major == 5 && minor == 1) || (major == 6 && minor == 0)))
        return FALSE;

    /* XP's NoUserNameInStartMenu policy also removes the user picture. */
    if (major == 5 && minor == 1 &&
        !RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                      0, KEY_QUERY_VALUE, &key))
    {
        LONG result = RegQueryValueExW(key, L"NoUserNameInStartMenu", NULL,
                                       &type, (BYTE *)&policy, &size);
        RegCloseKey(key);
        if (!result && type == REG_DWORD && policy) return FALSE;
    }
    return TRUE;
}

static void real_account_name( WCHAR *name, UINT count )
{
    DWORD size = count;

    DWORD len;

    if (GetUserNameW(name, &size) && name[0]) return;
    len = GetEnvironmentVariableW(L"USERNAME", name, count);
    if (len && len < count && name[0]) return;
    lstrcpynW(name, L"User", count);
}

static BOOL read_account_setting( const WCHAR *keyname, WCHAR *output, DWORD count )
{
    HKEY key;
    DWORD kind = 0, size = count * sizeof(WCHAR);
    LONG status;

    if (!count) return FALSE;
    output[0] = 0;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, account_key, 0, KEY_QUERY_VALUE, &key))
        return FALSE;
    status = RegQueryValueExW(key, keyname, NULL, &kind, (BYTE *)output, &size);
    RegCloseKey(key);
    if (status || kind != REG_SZ || size < sizeof(WCHAR) ||
        size > count * sizeof(WCHAR) || size % sizeof(WCHAR))
    {
        output[0] = 0;
        return FALSE;
    }
    /* REG_SZ can be malformed. Never read beyond the registry's byte count. */
    output[size / sizeof(WCHAR) - 1] = 0;
    return !!output[0];
}

void shell_account_display_name( WCHAR *name, UINT count )
{
    if (!read_account_setting(L"DisplayName", name, count))
        real_account_name(name, count);
}

static BOOL account_picture_policy(void)
{
    HKEY key;
    DWORD data = 0, type = 0, size = sizeof(data);
    LONG result;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer",
                      0, KEY_QUERY_VALUE, &key))
        return FALSE;
    result = RegQueryValueExW(key, L"UseDefaultTile", NULL, &type, (BYTE *)&data, &size);
    RegCloseKey(key);
    return !result && type == REG_DWORD && !!data;
}

static BOOL picture_is_valid( const WCHAR *path )
{
    DWORD attributes;

    if (lstrcmpiW(PathFindExtensionW(path), L".bmp")) return FALSE;
    attributes = GetFileAttributesW(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL common_picture_dir( WCHAR *folder )
{
    WCHAR data[MAX_PATH];

    if (!SHGetSpecialFolderPathW(NULL, data, CSIDL_COMMON_APPDATA, FALSE)) return FALSE;
    return !!PathCombineW(folder, data, L"Microsoft\\User Account Pictures");
}

static BOOL default_picture_dir( WCHAR *folder )
{
    WCHAR base[MAX_PATH];

    if (!common_picture_dir(base)) return FALSE;
    return !!PathCombineW(folder, base, L"Default Pictures");
}

/* Prefer the per-user chosen bitmap. Native OS assets are read in place:
 * XP may have <user>.bmp, Vista offers User.bmp. Never invent a random
 * initial assignment or copy vendor artwork into WATER source control. */
static BOOL resolve_account_picture( WCHAR *output )
{
    WCHAR folder[MAX_PATH], name[256], candidate[MAX_PATH];
    WORD major = 0, minor = 0;
    BOOL force_default = account_picture_policy();

    if (!force_default &&
        read_account_setting(L"Picture", output, MAX_PATH) && picture_is_valid(output))
        return TRUE;

    output[0] = 0;
    if (!common_picture_dir(folder)) return FALSE;
    get_shell_version(&major, &minor);

    if (!force_default)
    {
        real_account_name(name, ARRAY_SIZE(name));
        if (lstrlenW(name) + 5 < ARRAY_SIZE(name))
        {
            lstrcatW(name, L".bmp");
            if (PathCombineW(candidate, folder, name) && picture_is_valid(candidate))
            {
                lstrcpynW(output, candidate, MAX_PATH);
                return TRUE;
            }
        }
    }

    if (major == 6 && PathCombineW(candidate, folder, L"User.bmp") &&
        picture_is_valid(candidate))
    {
        lstrcpynW(output, candidate, MAX_PATH);
        return TRUE;
    }
    return FALSE;
}

static void draw_placeholder( HDC dc, const RECT *box )
{
    HBRUSH outer = CreateSolidBrush(RGB(106, 131, 163));
    HBRUSH inner = CreateSolidBrush(RGB(222, 233, 244));
    HBRUSH old_brush;

    FillRect(dc, box, outer);
    old_brush = SelectObject(dc, inner);
    Ellipse(dc, box->left + 16, box->top + 8, box->left + 33, box->top + 26);
    Ellipse(dc, box->left + 7, box->top + 29, box->left + 42, box->bottom + 12);
    SelectObject(dc, old_brush);
    DeleteObject(inner);
    DeleteObject(outer);
}

static void draw_account_picture( HDC dc, const RECT *rect )
{
    WCHAR path[MAX_PATH];
    HBITMAP bitmap = NULL;

    if (resolve_account_picture(path))
        bitmap = (HBITMAP)LoadImageW(NULL, path, IMAGE_BITMAP, 0, 0,
                                    LR_LOADFROMFILE | LR_CREATEDIBSECTION);
    if (bitmap)
    {
        BITMAP info;
        HDC bitmap_dc = CreateCompatibleDC(dc);

        if (bitmap_dc && GetObjectW(bitmap, sizeof(info), &info))
        {
            HGDIOBJ old = SelectObject(bitmap_dc, bitmap);
            SetStretchBltMode(dc, COLORONCOLOR);
            StretchBlt(dc, rect->left, rect->top, rect->right - rect->left,
                       rect->bottom - rect->top, bitmap_dc, 0, 0,
                       info.bmWidth, info.bmHeight, SRCCOPY);
            SelectObject(bitmap_dc, old);
        }
        else draw_placeholder(dc, rect);
        if (bitmap_dc) DeleteDC(bitmap_dc);
        DeleteObject(bitmap);
    }
    else draw_placeholder(dc, rect);
}

void paint_shell_account( HDC dc, const RECT *rect, BOOL start_screen )
{
    RECT avatar, text;
    WCHAR name[128];
    HFONT old_font;
    HBRUSH background;
    WORD major = 0, minor = 0;
    COLORREF color;

    shell_account_display_name(name, ARRAY_SIZE(name));
    get_shell_version(&major, &minor);

    if (!start_screen)
    {
        color = major == 5 ? RGB(28, 94, 182) : RGB(44, 48, 58);
        background = CreateSolidBrush(color);
        if (background)
        {
            FillRect(dc, rect, background);
            DeleteObject(background);
        }
    }

    avatar = *rect;
    avatar.top += (rect->bottom - rect->top - 48) / 2;
    avatar.bottom = avatar.top + 48;
    if (start_screen)
    {
        avatar.right = rect->right - 5;
        avatar.left = avatar.right - 48;
        text = *rect;
        text.right = avatar.left - 12;
    }
    else
    {
        avatar.left += 8;
        avatar.right = avatar.left + 48;
        text = *rect;
        text.left = avatar.right + 12;
    }
    draw_account_picture(dc, &avatar);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(255, 255, 255));
    old_font = SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(dc, name, -1, &text, DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS |
              (start_screen ? DT_RIGHT : DT_LEFT));
    SelectObject(dc, old_font);
}

struct account_dialog
{
    WCHAR picture[MAX_PATH];
    BOOL picture_locked;
};

static BOOL write_account_setting( HKEY key, const WCHAR *name, const WCHAR *value )
{
    LONG error;

    if (!value[0])
    {
        error = RegDeleteValueW(key, name);
        return !error || error == ERROR_FILE_NOT_FOUND;
    }
    error = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value,
                           (lstrlenW(value) + 1) * sizeof(WCHAR));
    return !error;
}

static BOOL save_account_dialog( HWND dialog, const struct account_dialog *state )
{
    WCHAR name[128], actual[128];
    HKEY key;
    LONG error;
    BOOL success;

    GetDlgItemTextW(dialog, IDC_ACCOUNT_DISPLAY_NAME, name, ARRAY_SIZE(name));
    real_account_name(actual, ARRAY_SIZE(actual));
    if (!lstrcmpW(name, actual)) name[0] = 0;

    error = RegCreateKeyExW(HKEY_CURRENT_USER, account_key, 0, NULL, 0,
                            KEY_SET_VALUE, NULL, &key, NULL);
    if (error) return FALSE;
    success = write_account_setting(key, L"DisplayName", name);
    if (!state->picture_locked)
        success = write_account_setting(key, L"Picture", state->picture) && success;
    RegCloseKey(key);
    return success;
}

static INT_PTR CALLBACK account_dialog_proc( HWND dialog, UINT msg, WPARAM wp, LPARAM lp )
{
    struct account_dialog *state = (struct account_dialog *)GetWindowLongPtrW(dialog, DWLP_USER);

    switch (msg)
    {
    case WM_INITDIALOG:
        state = (struct account_dialog *)lp;
        SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)state);
        read_account_setting(L"Picture", state->picture, ARRAY_SIZE(state->picture));
        state->picture_locked = account_picture_policy();
        {
            WCHAR name[128];
            shell_account_display_name(name, ARRAY_SIZE(name));
            SetDlgItemTextW(dialog, IDC_ACCOUNT_DISPLAY_NAME, name);
        }
        SetDlgItemTextW(dialog, IDC_ACCOUNT_PICTURE_PATH, state->picture);
        EnableWindow(GetDlgItem(dialog, IDC_ACCOUNT_PICTURE_BROWSE), !state->picture_locked);
        EnableWindow(GetDlgItem(dialog, IDC_ACCOUNT_PICTURE_RESET), !state->picture_locked);
        return TRUE;

    case WM_COMMAND:
        if (!state) break;
        switch (LOWORD(wp))
        {
        case IDC_ACCOUNT_PICTURE_BROWSE:
        {
            static const WCHAR filter[] = L"Bitmap pictures (*.bmp)\0*.bmp\0\0";
            OPENFILENAMEW open = {0};
            WCHAR folder[MAX_PATH] = {0};
            WCHAR file[MAX_PATH] = {0};

            default_picture_dir(folder);
            lstrcpynW(file, state->picture, ARRAY_SIZE(file));
            open.lStructSize = sizeof(open);
            open.hwndOwner = dialog;
            open.lpstrFilter = filter;
            open.lpstrFile = file;
            open.nMaxFile = ARRAY_SIZE(file);
            open.lpstrInitialDir = folder[0] ? folder : NULL;
            open.lpstrTitle = L"Choose an account picture";
            open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
            if (GetOpenFileNameW(&open) && picture_is_valid(file))
            {
                lstrcpynW(state->picture, file, ARRAY_SIZE(state->picture));
                SetDlgItemTextW(dialog, IDC_ACCOUNT_PICTURE_PATH, state->picture);
            }
            return TRUE;
        }
        case IDC_ACCOUNT_PICTURE_RESET:
            state->picture[0] = 0;
            SetDlgItemTextW(dialog, IDC_ACCOUNT_PICTURE_PATH, L"");
            return TRUE;

        case IDOK:
            if (!save_account_dialog(dialog, state))
            {
                MessageBoxW(dialog, L"Could not save your account preferences.",
                            L"Account Preferences", MB_ICONERROR | MB_OK);
                return TRUE;
            }
            EndDialog(dialog, IDOK);
            return TRUE;

        case IDCANCEL:
            EndDialog(dialog, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void edit_shell_account( HWND owner )
{
    struct account_dialog state = {0};

    DialogBoxParamW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(IDD_ACCOUNT_PREFERENCES),
                    owner, account_dialog_proc, (LPARAM)&state);
}
