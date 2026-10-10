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
#include "winnls.h"
#include "olectl.h"
#include "controls.h"

static HBRUSH hbrushPattern;
static HBITMAP hbitmapWallPaper;
static SIZE bitmapSize;
static BOOL fTileWallPaper;


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
        fTileWallPaper = GetProfileIntA( "desktop", "TileWallPaper", 0 );
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

    /* check for an owning thread; otherwise don't paint anything (non-desktop mode) */
    if (GetWindowThreadProcessId( hwnd, NULL ))
    {
        RECT rect;

        GetClientRect( hwnd, &rect );

        /* Paint desktop pattern (only if wall paper does not cover everything) */

        if (!hbitmapWallPaper ||
            (!fTileWallPaper && ((bitmapSize.cx < rect.right) || (bitmapSize.cy < rect.bottom))))
        {
            HBRUSH brush = hbrushPattern;
            if (!brush) brush = (HBRUSH)GetClassLongPtrW( hwnd, GCLP_HBRBACKGROUND );
            /* Set colors in case pattern is a monochrome bitmap */
            SetBkColor( hdc, RGB(0,0,0) );
            SetTextColor( hdc, GetSysColor(COLOR_BACKGROUND) );
            FillRect( hdc, &rect, brush );
        }

        /* Paint wall paper */

        if (hbitmapWallPaper)
        {
            INT x, y;
            HDC hMemDC = CreateCompatibleDC( hdc );

            SelectObject( hMemDC, hbitmapWallPaper );

            if (fTileWallPaper)
            {
                for (y = 0; y < rect.bottom; y += bitmapSize.cy)
                    for (x = 0; x < rect.right; x += bitmapSize.cx)
                        BitBlt( hdc, x, y, bitmapSize.cx, bitmapSize.cy, hMemDC, 0, 0, SRCCOPY );
            }
            else
            {
                x = (rect.left + rect.right - bitmapSize.cx) / 2;
                y = (rect.top + rect.bottom - bitmapSize.cy) / 2;
                if (x < 0) x = 0;
                if (y < 0) y = 0;
                BitBlt( hdc, x, y, bitmapSize.cx, bitmapSize.cy, hMemDC, 0, 0, SRCCOPY );
            }
            DeleteDC( hMemDC );
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
