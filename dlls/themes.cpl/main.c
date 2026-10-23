/* Copyright (C) 2026 Water contributors
 *
 * Original, limited Windows 98 Desktop Themes Control Panel compatibility.
 * This is not the Windows XP UxTheme visual-style engine.
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 */
#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "wingdi.h"
#include "winuser.h"
#include "commdlg.h"
#include "cpl.h"

#include "resource.h"
#include "rgb.h"

#define COLOR_COUNT 25
#define PROFILE_COLOR_SECTION "Control Panel\\Colors"
#define PROFILE_DESKTOP_SECTION "Control Panel\\Desktop"

static HINSTANCE module;

static const struct theme_color
{
    const char *name;
    int index;
} classic_colors[] =
{
    { "Scrollbar", COLOR_SCROLLBAR },
    { "Background", COLOR_BACKGROUND },
    { "ActiveTitle", COLOR_ACTIVECAPTION },
    { "InactiveTitle", COLOR_INACTIVECAPTION },
    { "Menu", COLOR_MENU },
    { "Window", COLOR_WINDOW },
    { "WindowFrame", COLOR_WINDOWFRAME },
    { "MenuText", COLOR_MENUTEXT },
    { "WindowText", COLOR_WINDOWTEXT },
    { "TitleText", COLOR_CAPTIONTEXT },
    { "ActiveBorder", COLOR_ACTIVEBORDER },
    { "InactiveBorder", COLOR_INACTIVEBORDER },
    { "AppWorkspace", COLOR_APPWORKSPACE },
    { "Hilight", COLOR_HIGHLIGHT },
    { "HilightText", COLOR_HIGHLIGHTTEXT },
    { "ButtonFace", COLOR_BTNFACE },
    { "ButtonShadow", COLOR_BTNSHADOW },
    { "GrayText", COLOR_GRAYTEXT },
    { "ButtonText", COLOR_BTNTEXT },
    { "InactiveTitleText", COLOR_INACTIVECAPTIONTEXT },
    { "ButtonHilight", COLOR_BTNHIGHLIGHT },
    { "ButtonDkShadow", COLOR_3DDKSHADOW },
    { "ButtonLight", COLOR_3DLIGHT },
    { "InfoText", COLOR_INFOTEXT },
    { "InfoWindow", COLOR_INFOBK },
};

static BOOL regular_file(const char *path)
{
    DWORD attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL read_profile(const char *section, const char *key, const char *theme,
                         char *buffer, DWORD capacity)
{
    DWORD size;

    SetLastError(ERROR_SUCCESS);
    size = GetPrivateProfileStringA(section, key, "", buffer, capacity, theme);
    return size < capacity - 1;
}

static BOOL expand_wallpaper(const char *theme, const char *source, char *result, DWORD capacity)
{
    char dir[MAX_PATH];
    char *last;
    DWORD size, len;

    size = GetFullPathNameA(theme, sizeof(dir), dir, NULL);
    if(!size || size >= sizeof(dir))
        return FALSE;
    last = strrchr(dir, '\\');
    if(!last)
        return FALSE;
    last[1] = 0;

    if(!strncmp(source, "%ThemeDir%", 10))
    {
        len = lstrlenA(dir);
        if(len + lstrlenA(source + 10) >= capacity)
            return FALSE;
        lstrcpyA(result, dir);
        lstrcatA(result, source + 10);
    }
    else if(strchr(source, '%'))
    {
        size = ExpandEnvironmentStringsA(source, result, capacity);
        if(!size || size > capacity)
            return FALSE;
    }
    else if((source[0] && source[1] == ':') ||
            (source[0] == '\\' && source[1] == '\\'))
    {
        if(lstrlenA(source) >= capacity)
            return FALSE;
        lstrcpyA(result, source);
    }
    else
    {
        len = lstrlenA(dir);
        if(len + lstrlenA(source) >= capacity)
            return FALSE;
        lstrcpyA(result, dir);
        lstrcatA(result, source);
    }
    return regular_file(result);
}

static BOOL apply_classic_theme(HWND parent, const char *theme)
{
    COLORREF colors[COLOR_COUNT];
    int indexes[COLOR_COUNT], count = 0;
    unsigned i;
    char value[128], wallpaper[MAX_PATH * 2], wallpaper_path[MAX_PATH * 2];
    char tile[8], style[8], text[96];
    DWORD length;
    HKEY key;

    if(!regular_file(theme))
        goto invalid;

    for(i = 0; i < sizeof(classic_colors) / sizeof(classic_colors[0]); i++)
    {
        unsigned char rgb[3];
        if(!read_profile(PROFILE_COLOR_SECTION, classic_colors[i].name, theme,
                         value, sizeof(value)))
            goto invalid;
        if(!value[0])
            continue;
        if(!theme_parse_rgb(value, rgb))
            goto invalid;
        colors[count] = RGB(rgb[0], rgb[1], rgb[2]);
        indexes[count++] = classic_colors[i].index;
    }

    if(!read_profile(PROFILE_DESKTOP_SECTION, "Wallpaper", theme,
                     wallpaper, sizeof(wallpaper)) ||
       !read_profile(PROFILE_DESKTOP_SECTION, "TileWallpaper", theme,
                     tile, sizeof(tile)) ||
       !read_profile(PROFILE_DESKTOP_SECTION, "WallpaperStyle", theme,
                     style, sizeof(style)))
        goto invalid;

    if(tile[0] && lstrcmpA(tile, "0") && lstrcmpA(tile, "1"))
        goto invalid;
    if(style[0] && lstrcmpA(style, "0") && lstrcmpA(style, "2"))
        goto invalid;

    wallpaper_path[0] = 0;
    if(wallpaper[0] && !expand_wallpaper(theme, wallpaper, wallpaper_path,
                                         sizeof(wallpaper_path)))
        goto invalid;
    if(!count && !wallpaper_path[0])
        goto invalid;

    /* Do not modify user settings until the file has been validated. */
    if(wallpaper_path[0])
    {
        if(RegCreateKeyExA(HKEY_CURRENT_USER, "Control Panel\\Desktop", 0, NULL, 0,
                           KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
            goto failed;
        if(tile[0])
        {
            LONG res = RegSetValueExA(key, "TileWallpaper", 0, REG_SZ, (const BYTE *)tile,
                                      lstrlenA(tile) + 1);
            if(res != ERROR_SUCCESS)
            {
                RegCloseKey(key);
                SetLastError(res);
                goto failed;
            }
        }
        if(style[0])
        {
            LONG res = RegSetValueExA(key, "WallpaperStyle", 0, REG_SZ, (const BYTE *)style,
                                      lstrlenA(style) + 1);
            if(res != ERROR_SUCCESS)
            {
                RegCloseKey(key);
                SetLastError(res);
                goto failed;
            }
        }
        RegCloseKey(key);

        if(!SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, wallpaper_path,
                                  SPIF_UPDATEINIFILE | SPIF_SENDCHANGE))
            goto failed;
    }

    if(count)
    {
        if(RegCreateKeyExA(HKEY_CURRENT_USER, "Control Panel\\Colors", 0, NULL, 0,
                           KEY_SET_VALUE, NULL, &key, NULL) != ERROR_SUCCESS)
            goto failed;

        for(i = 0; i < (unsigned)count; i++)
        {
            const char *name;
            unsigned c;
            for(c = 0; c < sizeof(classic_colors) / sizeof(classic_colors[0]); c++)
                if(classic_colors[c].index == indexes[i]) break;
            name = c < sizeof(classic_colors) / sizeof(classic_colors[0]) ?
                   classic_colors[c].name : NULL;
            if(!name) continue;
            wsprintfA(text, "%u %u %u", GetRValue(colors[i]), GetGValue(colors[i]), GetBValue(colors[i]));
            {
                LONG res = RegSetValueExA(key, name, 0, REG_SZ, (const BYTE *)text,
                                          lstrlenA(text) + 1);
                if(res != ERROR_SUCCESS)
                {
                    RegCloseKey(key);
                    SetLastError(res);
                    goto failed;
                }
            }
        }
        RegCloseKey(key);

        if(!SetSysColors(count, indexes, colors))
            goto failed;
    }

    MessageBoxA(parent, "The supported desktop-theme settings have been applied.",
                "Desktop Themes", MB_OK | MB_ICONINFORMATION);
    return TRUE;

invalid:
    MessageBoxA(parent, "The selected .theme file has missing or invalid supported settings, "
                       "or its wallpaper cannot be found. No settings were applied.",
                "Desktop Themes", MB_OK | MB_ICONWARNING);
    return FALSE;
failed:
    length = GetLastError();
    wsprintfA(text, "Windows could not apply all theme settings (error %lu). "
                     "Some settings may already have changed.", length);
    MessageBoxA(parent, text, "Desktop Themes", MB_OK | MB_ICONWARNING);
    return FALSE;
}

static void open_theme(HWND parent)
{
    OPENFILENAMEA ofn;
    char file[MAX_PATH] = "";

    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.hInstance = module;
    ofn.lpstrFilter = "Classic Desktop Themes (*.theme)\0*.theme\0All files (*.*)\0*.*\0";
    ofn.lpstrFile = file;
    ofn.nMaxFile = sizeof(file);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
    ofn.lpstrDefExt = "theme";

    if(!GetOpenFileNameA(&ofn))
        return;
    if(MessageBoxA(parent, "Apply the supported colors and wallpaper from this theme? "
                          "This will modify your current desktop settings.",
                   "Desktop Themes", MB_YESNO | MB_ICONQUESTION) == IDYES)
        apply_classic_theme(parent, file);
}

LONG CALLBACK CPlApplet(HWND parent, UINT command, LPARAM param1, LPARAM param2)
{
    switch(command)
    {
    case CPL_INIT:
        return TRUE;
    case CPL_GETCOUNT:
        return 1;
    case CPL_INQUIRE:
    {
        CPLINFO *info = (CPLINFO *)param2;
        if(!info) return FALSE;
        info->idIcon = ICO_THEME;
        info->idName = IDS_CPL_NAME;
        info->idInfo = IDS_CPL_INFO;
        info->lData = 0;
        return 0;
    }
    case CPL_DBLCLK:
        open_theme(parent);
        return 0;
    }
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
    if(reason == DLL_PROCESS_ATTACH)
    {
        module = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
