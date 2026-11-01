/*
 * Desktop window class.
 *
 * Copyright 1994 Alexandre Julliard
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winreg.h"
#include "winnls.h"
#include "olectl.h"
#include "controls.h"

static HBRUSH hbrushPattern;
static HBITMAP hbitmapWallPaper;
static SIZE bitmapSize;

enum wallpaper_position
{
    WALLPAPER_CENTER,
    WALLPAPER_TILE,
    WALLPAPER_STRETCH,
    WALLPAPER_FIT,
    WALLPAPER_FILL
};
static enum wallpaper_position wallpaper_position;

/* Read the modern per-user REG_SZ style values without modifying them.
 * A missing value is different from a valid "0" (center/tile). */
static BOOL DESKTOP_GetStyleValue( HKEY key, const WCHAR *name, WCHAR *value, DWORD chars )
{
    DWORD type, bytes = chars * sizeof(WCHAR);

    if (RegQueryValueExW( key, name, NULL, &type, (BYTE *)value, &bytes ) ||
        type != REG_SZ || bytes < sizeof(WCHAR) || bytes > chars * sizeof(WCHAR) ||
        (bytes % sizeof(WCHAR)) || value[bytes / sizeof(WCHAR) - 1])
        return FALSE;
    return TRUE;
}

/* Windows 3.1 used WIN.INI [desktop] TileWallpaper=0/1. Later versions
 * use HKCU\\Control Panel\\Desktop WallpaperStyle and TileWallpaper:
 * 0=center/tile, 2=stretch, 6=fit and 10=fill. Fit and fill are Win7+
 * display modes, not features of the original 1992 Control Panel. */
static void DESKTOP_UpdateWallpaperPosition(void)
{
    HKEY key;
    WCHAR style[32], tile[32];
    BOOL have_style = FALSE, have_tile = FALSE;

    wallpaper_position = GetProfileIntA( "desktop", "TileWallpaper", 0 ) ?
                         WALLPAPER_TILE : WALLPAPER_CENTER;
    if (!RegOpenKeyExW( HKEY_CURRENT_USER, L"Control Panel\\Desktop", 0,
                        KEY_QUERY_VALUE, &key ))
    {
        have_style = DESKTOP_GetStyleValue( key, L"WallpaperStyle",
                                            style, ARRAY_SIZE(style) );
        have_tile = DESKTOP_GetStyleValue( key, L"TileWallpaper",
                                           tile, ARRAY_SIZE(tile) );
        RegCloseKey( key );

        if (have_style && !lstrcmpW( style, L"10" ))
            wallpaper_position = WALLPAPER_FILL;
        else if (have_style && !lstrcmpW( style, L"6" ))
            wallpaper_position = WALLPAPER_FIT;
        else if (have_style && !lstrcmpW( style, L"2" ))
            wallpaper_position = WALLPAPER_STRETCH;
        else if (have_tile && !lstrcmpW( tile, L"1" ))
            wallpaper_position = WALLPAPER_TILE;
        else if (have_tile && !lstrcmpW( tile, L"0" ))
            wallpaper_position = WALLPAPER_CENTER;
    }
}

/* Pure placement arithmetic, also used by the drawing path for clipping.
 * Work in 64-bit intermediates so even large bitmaps cannot overflow when
 * scaled against the desktop dimensions. No upscale is invented for center. */
static void DESKTOP_GetWallpaperRects( enum wallpaper_position position,
                                      const RECT *desktop, const SIZE *bitmap,
                                      RECT *dst, RECT *src )
{
    const INT width = desktop->right - desktop->left;
    const INT height = desktop->bottom - desktop->top;
    LONGLONG size;

    *dst = *desktop;
    src->left = src->top = 0;
    src->right = bitmap->cx;
    src->bottom = bitmap->cy;

    switch (position)
    {
    case WALLPAPER_CENTER:
        dst->left += (width - bitmap->cx) / 2;
        dst->top += (height - bitmap->cy) / 2;
        dst->right = dst->left + bitmap->cx;
        dst->bottom = dst->top + bitmap->cy;
        break;
    case WALLPAPER_FIT:
        if ((LONGLONG)bitmap->cx * height > (LONGLONG)bitmap->cy * width)
        {
            size = (LONGLONG)bitmap->cy * width / bitmap->cx;
            if (size < 1) size = 1;
            dst->top += (height - size) / 2;
            dst->bottom = dst->top + size;
        }
        else
        {
            size = (LONGLONG)bitmap->cx * height / bitmap->cy;
            if (size < 1) size = 1;
            dst->left += (width - size) / 2;
            dst->right = dst->left + size;
        }
        break;
    case WALLPAPER_FILL:
        if ((LONGLONG)bitmap->cx * height > (LONGLONG)bitmap->cy * width)
        {
            size = (LONGLONG)bitmap->cy * width / height;
            if (size < 1) size = 1;
            src->left = (bitmap->cx - size) / 2;
            src->right = src->left + size;
        }
        else
        {
            size = (LONGLONG)bitmap->cx * height / width;
            if (size < 1) size = 1;
            src->top = (bitmap->cy - size) / 2;
            src->bottom = src->top + size;
        }
        break;
    case WALLPAPER_STRETCH:
    case WALLPAPER_TILE:
        break;
    }
}


/***********************************************************************
 *           DESKTOP_LoadPicture
 *
 * LoadImageW supports BMP wallpaper, but not JPEG. Reuse OLE's picture
 * decoder on demand instead of adding another decoder to user32 (which
 * is itself a dependency of oleaut32).
 */
static HBITMAP DESKTOP_LoadPicture( const WCHAR *filename )
{
    HRESULT (WINAPI *load_picture)(LPOLESTR, LPUNKNOWN, DWORD, OLE_COLOR, REFIID, LPVOID *);
    WCHAR full_path[MAX_PATH];
    IPicture *picture = NULL;
    HMODULE module;
    HBITMAP bitmap = NULL;
    OLE_HANDLE handle;
    DWORD length;
    short type;

    /* Resolve relative paths before passing them to the picture loader,
     * which otherwise treats such names as URL monikers. */
    length = GetFullPathNameW( filename, ARRAY_SIZE(full_path), full_path, NULL );
    if (!length || length >= ARRAY_SIZE(full_path)) return NULL;

    module = LoadLibraryW( L"oleaut32.dll" );
    if (!module) return NULL;

    load_picture = (void *)GetProcAddress( module, "OleLoadPicturePath" );
    if (load_picture &&
        SUCCEEDED(load_picture( full_path, NULL, 0, 0, &IID_IPicture, (void **)&picture )) &&
        picture)
    {
        if (SUCCEEDED(IPicture_get_Type( picture, &type )) && type == PICTYPE_BITMAP &&
            SUCCEEDED(IPicture_get_Handle( picture, &handle )) && handle)
            bitmap = CopyImage( UlongToHandle(handle), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION );
    }
    if (picture) IPicture_Release( picture );
    FreeLibrary( module );
    return bitmap;
}

/***********************************************************************
 *           DESKTOP_LoadBitmap
 */
static HBITMAP DESKTOP_LoadBitmap( const WCHAR *filename )
{
    WCHAR buffer[MAX_PATH];
    HBITMAP bitmap;
    UINT length;

    if (!filename || !filename[0]) return NULL;

    bitmap = LoadImageW( 0, filename, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE );
    if (!bitmap) bitmap = DESKTOP_LoadPicture( filename );
    if (bitmap) return bitmap;

    /* Legacy wallpaper names can be relative to the Windows directory.
     * Do not index before the buffer if GetWindowsDirectoryW() fails. */
    length = GetWindowsDirectoryW( buffer, ARRAY_SIZE(buffer) );
    if (!length || length >= ARRAY_SIZE(buffer)) return NULL;
    if (buffer[length - 1] != '\\')
    {
        if (length + 1 >= ARRAY_SIZE(buffer)) return NULL;
        buffer[length++] = '\\';
    }
    if (lstrlenW( filename ) >= ARRAY_SIZE(buffer) - length) return NULL;
    lstrcpyW( buffer + length, filename );

    bitmap = LoadImageW( 0, buffer, IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION | LR_LOADFROMFILE );
    if (!bitmap) bitmap = DESKTOP_LoadPicture( buffer );
    return bitmap;
}

/***********************************************************************
 *           init_wallpaper
 */
static void init_wallpaper( const WCHAR *wallpaper )
{
    HBITMAP hbitmap = DESKTOP_LoadBitmap( wallpaper );

    if (hbitmapWallPaper) DeleteObject( hbitmapWallPaper );
    hbitmapWallPaper = hbitmap;
    bitmapSize.cx = bitmapSize.cy = 0;
    DESKTOP_UpdateWallpaperPosition();
    if (hbitmap)
    {
	BITMAP bmp;
        if (!GetObjectW( hbitmap, sizeof(bmp), &bmp ))
        {
            DeleteObject( hbitmapWallPaper );
            hbitmapWallPaper = NULL;
            bitmapSize.cx = bitmapSize.cy = 0;
            return;
        }
        bitmapSize.cx = bmp.bmWidth ? bmp.bmWidth : 1;
        bitmapSize.cy = bmp.bmHeight ? bmp.bmHeight : 1;
    }
}

/***********************************************************************
 *           DesktopWndProcA
 */
LRESULT WINAPI DesktopWndProcA( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch (message)
    {
    case WM_NCCREATE:
    case WM_NCCALCSIZE:
    case WM_PARENTNOTIFY:
    case WM_DISPLAYCHANGE:
        return NtUserMessageCall( hwnd, message, wParam, lParam, 0, NtUserDesktopWindowProc, TRUE );

    default:
        if (message < WM_USER)
            return DefWindowProcA( hwnd, message, wParam, lParam );
        return NtUserMessageCall( hwnd, message, wParam, lParam, 0, NtUserDesktopWindowProc, TRUE );
    }
}

/***********************************************************************
 *           DesktopWndProcW
 */
LRESULT WINAPI DesktopWndProcW( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    switch (message)
    {
    case WM_NCCREATE:
    case WM_NCCALCSIZE:
    case WM_PARENTNOTIFY:
    case WM_DISPLAYCHANGE:
        return NtUserMessageCall( hwnd, message, wParam, lParam, 0, NtUserDesktopWindowProc, FALSE );

    default:
        if (message < WM_USER)
            return DefWindowProcW( hwnd, message, wParam, lParam );
        return NtUserMessageCall( hwnd, message, wParam, lParam, 0, NtUserDesktopWindowProc, FALSE );
    }
}

/***********************************************************************
 *           PaintDesktop   (USER32.@)
 *
 */
BOOL WINAPI PaintDesktop(HDC hdc)
{
    HWND hwnd = GetDesktopWindow();

    /* A non-desktop process must not paint the host's background. */
    if (GetWindowThreadProcessId( hwnd, NULL ))
    {
        RECT rect;
        INT width, height;

        if (!GetClientRect( hwnd, &rect )) return FALSE;
        width = rect.right - rect.left;
        height = rect.bottom - rect.top;
        if (width <= 0 || height <= 0) return TRUE;

        /* Center and fit can leave a border. Stretch, fill and tile cover
         * the entire client area when a wallpaper bitmap is available. */
        if (!hbitmapWallPaper || wallpaper_position == WALLPAPER_CENTER ||
            wallpaper_position == WALLPAPER_FIT)
        {
            HBRUSH brush = hbrushPattern;
            if (!brush) brush = (HBRUSH)GetClassLongPtrW( hwnd, GCLP_HBRBACKGROUND );
            SetBkColor( hdc, RGB(0,0,0) );
            SetTextColor( hdc, GetSysColor(COLOR_BACKGROUND) );
            FillRect( hdc, &rect, brush );
        }

        if (hbitmapWallPaper && bitmapSize.cx > 0 && bitmapSize.cy > 0)
        {
            HDC memdc = CreateCompatibleDC( hdc );
            HGDIOBJ old_bitmap;
            RECT dst, src;

            if (!memdc) return FALSE;
            old_bitmap = SelectObject( memdc, hbitmapWallPaper );
            if (!old_bitmap || old_bitmap == HGDI_ERROR)
            {
                DeleteDC( memdc );
                return FALSE;
            }

            if (wallpaper_position == WALLPAPER_TILE)
            {
                INT x, y;
                for (y = rect.top; y < rect.bottom; y += bitmapSize.cy)
                    for (x = rect.left; x < rect.right; x += bitmapSize.cx)
                        BitBlt( hdc, x, y, bitmapSize.cx, bitmapSize.cy,
                                memdc, 0, 0, SRCCOPY );
            }
            else
            {
                DESKTOP_GetWallpaperRects( wallpaper_position, &rect, &bitmapSize, &dst, &src );
                if (wallpaper_position == WALLPAPER_CENTER)
                    BitBlt( hdc, dst.left, dst.top, bitmapSize.cx, bitmapSize.cy,
                            memdc, 0, 0, SRCCOPY );
                else
                    StretchBlt( hdc, dst.left, dst.top, dst.right - dst.left,
                                dst.bottom - dst.top, memdc, src.left, src.top,
                                src.right - src.left, src.bottom - src.top, SRCCOPY );
            }
            SelectObject( memdc, old_bitmap );
            DeleteDC( memdc );
        }
    }
    return TRUE;
}

/***********************************************************************
 *           SetDeskWallpaper   (USER32.@)
 */
BOOL WINAPI SetDeskWallpaper( const char *filename )
{
    return SystemParametersInfoA( SPI_SETDESKWALLPAPER, MAX_PATH, (void *)filename, SPIF_UPDATEINIFILE );
}

/***********************************************************************
 *           update_wallpaper
 */
BOOL update_wallpaper( const WCHAR *wallpaper, const WCHAR *pattern )
{
    int pat[8];

    if (hbrushPattern) DeleteObject( hbrushPattern );
    hbrushPattern = 0;
    memset( pat, 0, sizeof(pat) );
    if (pattern)
    {
        char buffer[64];
        WideCharToMultiByte( CP_ACP, 0, pattern, -1, buffer, sizeof(buffer), NULL, NULL );
        if (sscanf( buffer, " %d %d %d %d %d %d %d %d",
                    &pat[0], &pat[1], &pat[2], &pat[3],
                    &pat[4], &pat[5], &pat[6], &pat[7] ))
        {
            WORD ptrn[8];
            HBITMAP hbitmap;
            int i;

            for (i = 0; i < 8; i++) ptrn[i] = pat[i] & 0xffff;
            hbitmap = CreateBitmap( 8, 8, 1, 1, ptrn );
            hbrushPattern = CreatePatternBrush( hbitmap );
            DeleteObject( hbitmap );
        }
    }
    init_wallpaper( wallpaper );
    NtUserRedrawWindow( GetDesktopWindow(), 0, 0, RDW_INVALIDATE | RDW_ERASE | RDW_NOCHILDREN );
    return TRUE;
}
