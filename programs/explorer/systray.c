/*
 * Copyright (C) 2004 Mike Hearn, for CodeWeavers
 * Copyright (C) 2005 Robert Shearman
 * Copyright (C) 2008 Alexandre Julliard
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

#include <assert.h>
#include <stdlib.h>

#include <windows.h>
#include <ntuser.h>
#include <commctrl.h>
#include <shellapi.h>

#include <wine/debug.h>
#include <wine/list.h>

#include "explorer_private.h"
#include "wine/win16_profile.h"
#include "resource.h"

WINE_DEFAULT_DEBUG_CHANNEL(systray);

#define TRAY_MINIMIZE_ALL 419
#define TRAY_MINIMIZE_ALL_UNDO 416
#define TASKBAR_CLOCK_TIMER 3
#define TASKBAR_LOCK_COMMAND 6100
#define TASKBAR_CLOCK_COMMAND 6101
#define TASKBAR_QUICKLAUNCH_COMMAND 6102

enum taskbar_visual_style
{
    TASKBAR_VISUAL_CLASSIC,
    TASKBAR_VISUAL_LUNA_BLUE,
    TASKBAR_VISUAL_VISTA_AERO
};

struct notify_data_icon
{
    /* data for the icon bitmap */
    UINT width;
    UINT height;
    UINT planes;
    UINT bpp;
};

struct notify_data  /* platform-independent format for NOTIFYICONDATA */
{
    LONG  hWnd;
    UINT  uID;
    UINT  uFlags;
    UINT  uCallbackMessage;
    struct notify_data_icon icon_info; /* systray icon bitmap info */
    WCHAR szTip[128];
    DWORD dwState;
    DWORD dwStateMask;
    WCHAR szInfo[256];
    union {
        UINT uTimeout;
        UINT uVersion;
    } u;
    WCHAR szInfoTitle[64];
    DWORD dwInfoFlags;
    GUID  guidItem;
    struct notify_data_icon balloon_icon_info; /* balloon icon bitmap info */
    BYTE icon_data[];
};

#define ICON_DISPLAY_HIDDEN -1
#define ICON_DISPLAY_DOCKED -2

/* an individual systray icon, unpacked from the NOTIFYICONDATA and always in unicode */
struct icon
{
    struct list    entry;
    HICON          image;    /* the image to render */
    HWND           owner;    /* the HWND passed in to the Shell_NotifyIcon call */
    HWND           window;   /* the adaptor window */
    BOOL           layered;  /* whether we are using a layered window */
    HWND           tooltip;  /* Icon tooltip */
    UINT           state;    /* state flags */
    UINT           id;       /* the unique id given by the app */
    UINT           callback_message;
    int            display;  /* index in display list, or -1 if hidden */
    WCHAR          tiptext[128]; /* Tooltip text. If empty => tooltip disabled */
    WCHAR          info_text[256];  /* info balloon text */
    WCHAR          info_title[64];  /* info balloon title */
    UINT           info_flags;      /* flags for info balloon */
    UINT           info_timeout;    /* timeout for info balloon */
    HICON          info_icon;       /* info balloon icon */
    UINT           version;         /* notify icon api version */
};

static struct list icon_list = LIST_INIT( icon_list );

struct taskbar_button
{
    struct list entry;
    HWND        hwnd;
    HWND        button;
    WCHAR       executable[MAX_PATH]; /* App image, never a guessed title match. */
    BOOL        active;
    BOOL        visible;
};

static struct list taskbar_buttons = LIST_INIT( taskbar_buttons );

static HWND tray_window;
static HWND clock_window;
static enum taskbar_visual_style taskbar_visual;
static HBRUSH taskbar_clock_brush;
static UINT taskbar_edge = ABE_BOTTOM;
static BOOL taskbar_locked = TRUE;
static BOOL taskbar_show_clock = TRUE;
static BOOL taskbar_dragging;
static POINT taskbar_drag_start;

static unsigned int nb_displayed;

static BOOL enable_taskbar; /* show full taskbar, with dedicated systray area */
static BOOL show_systray; /* show a standalone systray window */
static BOOL enable_dock; /* allow systray icons to be docked in the host systray */
static BOOL no_tray_items; /* hide the systray and all systray icons */

static int icon_cx, icon_cy, tray_width, tray_height;
static int start_button_width, start_button_height;
static int taskbar_button_width, taskbar_button_height;
static int clock_width, clock_height;
static WCHAR start_label[50];

static struct icon *balloon_icon;
static HWND balloon_window;
static POINT balloon_pos;

#define MIN_DISPLAYED 8
#define ICON_BORDER  2

#define BALLOON_CREATE_TIMER 1
#define BALLOON_SHOW_TIMER   2

#define BALLOON_CREATE_TIMEOUT   2000
#define BALLOON_SHOW_MIN_TIMEOUT 10000
#define BALLOON_SHOW_MAX_TIMEOUT 30000

#define WM_POPUPSYSTEMMENU  0x0313

static LRESULT WINAPI shell_traywnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );
static LRESULT WINAPI tray_icon_wndproc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );

static void show_icon( struct icon *icon );
static void hide_icon( struct icon *icon );
static BOOL delete_icon( struct icon *icon );

static WNDCLASSEXW shell_traywnd_class =
{
    .cbSize = sizeof(WNDCLASSEXW),
    .style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
    .lpfnWndProc = shell_traywnd_proc,
    .hbrBackground = (HBRUSH)COLOR_WINDOW,
    .lpszClassName = L"Shell_TrayWnd",
};
static WNDCLASSEXW tray_icon_class =
{
    .cbSize = sizeof(WNDCLASSEXW),
    .style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS,
    .lpfnWndProc = tray_icon_wndproc,
    .lpszClassName = L"__wine_tray_icon",
};

static void do_hide_systray(void);
static void do_show_systray(void);

/* Retrieves icon record by owner window and ID */
static struct icon *get_icon(HWND owner, UINT id)
{
    struct icon *this;

    /* search for the icon */
    LIST_FOR_EACH_ENTRY( this, &icon_list, struct icon, entry )
        if ((this->id == id) && (this->owner == owner)) return this;

    return NULL;
}

static void init_common_controls(void)
{
    static BOOL initialized = FALSE;

    if (!initialized)
    {
        INITCOMMONCONTROLSEX init_tooltip;

        init_tooltip.dwSize = sizeof(INITCOMMONCONTROLSEX);
        init_tooltip.dwICC = ICC_TAB_CLASSES|ICC_STANDARD_CLASSES;

        InitCommonControlsEx(&init_tooltip);
        initialized = TRUE;
    }
}

/* Creates tooltip window for icon. */
static void create_tooltip(struct icon *icon)
{
    TTTOOLINFOW ti;

    init_common_controls();
    icon->tooltip = CreateWindowExW( WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL, WS_POPUP | TTS_ALWAYSTIP,
                                     CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                     icon->window, NULL, NULL, NULL );

    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize = sizeof(TTTOOLINFOW);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = icon->window;
    ti.uId = (UINT_PTR)icon->window;
    ti.lpszText = icon->tiptext;
    SendMessageW(icon->tooltip, TTM_ADDTOOLW, 0, (LPARAM)&ti);
}

static void set_balloon_position( struct icon *icon )
{
    RECT rect;
    POINT pos;

    GetWindowRect( icon->window, &rect );
    pos.x = (rect.left + rect.right) / 2;
    pos.y = (rect.top + rect.bottom) / 2;
    SendMessageW( balloon_window, TTM_TRACKPOSITION, 0, MAKELONG( pos.x, pos.y ));
}

static void update_systray_balloon_position(void)
{
    RECT rect;
    POINT pos;

    if (!balloon_icon) return;
    GetWindowRect( balloon_icon->window, &rect );
    pos.x = (rect.left + rect.right) / 2;
    pos.y = (rect.top + rect.bottom) / 2;
    if (pos.x == balloon_pos.x && pos.y == balloon_pos.y) return; /* nothing changed */
    balloon_pos = pos;
    SendMessageW( balloon_window, TTM_TRACKPOSITION, 0, MAKELONG( pos.x, pos.y ));
}

static void balloon_create_timer( struct icon *icon )
{
    TTTOOLINFOW ti;

    init_common_controls();
    balloon_window = CreateWindowExW( WS_EX_TOPMOST, TOOLTIPS_CLASSW, NULL,
                                      WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX | TTS_BALLOON | TTS_CLOSE,
                                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                      icon->window, NULL, NULL, NULL );

    memset( &ti, 0, sizeof(ti) );
    ti.cbSize = sizeof(TTTOOLINFOW);
    ti.hwnd = icon->window;
    ti.uId = (UINT_PTR)icon->window;
    ti.uFlags = TTF_TRACK | TTF_IDISHWND;
    ti.lpszText = icon->info_text;
    SendMessageW( balloon_window, TTM_ADDTOOLW, 0, (LPARAM)&ti );
    if ((icon->info_flags & NIIF_ICONMASK) == NIIF_USER)
    {
        SendMessageW( balloon_window, TTM_SETTITLEW, (WPARAM)icon->info_icon, (LPARAM)icon->info_title );
    }
    else
    {
        UINT info_flags_wparam = icon->info_flags & NIIF_ERROR;

        if (icon->info_flags & NIIF_LARGEICON)
            info_flags_wparam += TTI_ERROR;
        SendMessageW( balloon_window, TTM_SETTITLEW, info_flags_wparam, (LPARAM)icon->info_title );
    }
    balloon_icon = icon;
    balloon_pos.x = balloon_pos.y = MAXLONG;
    update_systray_balloon_position();
    SendMessageW( balloon_window, TTM_TRACKACTIVATE, TRUE, (LPARAM)&ti );
    KillTimer( icon->window, BALLOON_CREATE_TIMER );
    SetTimer( icon->window, BALLOON_SHOW_TIMER, icon->info_timeout, NULL );
}

static BOOL show_balloon( struct icon *icon )
{
    if (!enable_taskbar && !show_systray) return FALSE;  /* systray has been hidden */
    if (icon->display == ICON_DISPLAY_HIDDEN) return FALSE;  /* not displayed */
    if (!icon->info_text[0]) return FALSE;  /* no balloon */
    balloon_icon = icon;
    SetTimer( icon->window, BALLOON_CREATE_TIMER, BALLOON_CREATE_TIMEOUT, NULL );
    return TRUE;
}

static void hide_balloon( struct icon *icon )
{
    if (!balloon_icon) return;
    if (balloon_window)
    {
        KillTimer( balloon_icon->window, BALLOON_SHOW_TIMER );
        DestroyWindow( balloon_window );
        balloon_window = 0;
    }
    else KillTimer( balloon_icon->window, BALLOON_CREATE_TIMER );
    balloon_icon = NULL;
}

static void show_next_balloon(void)
{
    struct icon *icon;

    LIST_FOR_EACH_ENTRY( icon, &icon_list, struct icon, entry )
        if (show_balloon( icon )) break;
}

static void update_balloon( struct icon *icon )
{
    if (balloon_icon == icon)
    {
        hide_balloon( icon );
        show_balloon( icon );
    }
    else if (!balloon_icon)
    {
        show_balloon( icon );
    }
}

static void balloon_timer( struct icon *icon )
{
    icon->info_text[0] = 0;  /* clear text now that balloon has been shown */
    hide_balloon( icon );
    show_next_balloon();
}

/* Synchronize tooltip text with tooltip window */
static void update_tooltip_text(struct icon *icon)
{
    TTTOOLINFOW ti;

    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize = sizeof(TTTOOLINFOW);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = icon->window;
    ti.uId = (UINT_PTR)icon->window;
    ti.lpszText = icon->tiptext;

    SendMessageW(icon->tooltip, TTM_UPDATETIPTEXTW, 0, (LPARAM)&ti);
}

/* get the position of an icon in the stand-alone tray */
static POINT get_icon_pos( struct icon *icon )
{
    POINT pos;

    if (enable_taskbar)
    {
        if (taskbar_edge == ABE_LEFT || taskbar_edge == ABE_RIGHT)
        {
            pos.x = (tray_width - icon_cx) / 2;
            pos.y = tray_height - clock_height - icon_cy * (icon->display + 1);
        }
        else
        {
            pos.x = tray_width - clock_width - icon_cx * (icon->display + 1);
            pos.y = (tray_height - icon_cy) / 2;
        }
    }
    else
    {
        pos.x = icon_cx * icon->display;
        pos.y = 0;
    }

    return pos;
}

/* Reposition existing notification icons after the taskbar size changes. */
static void layout_systray_icons(void)
{
    struct icon *icon;

    LIST_FOR_EACH_ENTRY( icon, &icon_list, struct icon, entry )
    {
        POINT pos;

        if (!icon->window || icon->display < 0) continue;
        pos = get_icon_pos( icon );
        SetWindowPos( icon->window, 0, pos.x, pos.y, 0, 0,
                      SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER );
    }
}

/* get the size of the stand-alone tray window */
static SIZE get_window_size(void)
{
    SIZE size;
    RECT rect;

    rect.left = 0;
    rect.top = 0;
    rect.right = icon_cx * max( nb_displayed, MIN_DISPLAYED );
    rect.bottom = icon_cy;
    AdjustWindowRect( &rect, WS_CAPTION, FALSE );
    size.cx = rect.right - rect.left;
    size.cy = rect.bottom - rect.top;
    return size;
}

/* synchronize tooltip position with tooltip window */
static void update_tooltip_position( struct icon *icon )
{
    TTTOOLINFOW ti;

    ZeroMemory(&ti, sizeof(ti));
    ti.cbSize = sizeof(TTTOOLINFOW);
    ti.uFlags = TTF_SUBCLASS | TTF_IDISHWND;
    ti.hwnd = icon->window;
    ti.uId = (UINT_PTR)icon->window;
    ti.lpszText = icon->tiptext;
    SendMessageW( icon->tooltip, TTM_NEWTOOLRECTW, 0, (LPARAM)&ti );
    if (balloon_icon == icon) set_balloon_position( icon );
}

static void paint_layered_icon( struct icon *icon )
{
    BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    int width = GetSystemMetrics( SM_CXSMICON );
    int height = GetSystemMetrics( SM_CYSMICON );
    BITMAPINFO *info;
    HBITMAP dib, mask;
    HDC hdc;
    RECT rc;
    SIZE size;
    POINT pos;
    int i, x, y;
    void *color_bits, *mask_bits;
    DWORD *ptr;
    BOOL has_alpha = FALSE;

    GetWindowRect( icon->window, &rc );
    size.cx = rc.right - rc.left;
    size.cy = rc.bottom - rc.top;
    pos.x = (size.cx - width) / 2;
    pos.y = (size.cy - height) / 2;

    if (!(info = calloc( 1, FIELD_OFFSET( BITMAPINFO, bmiColors[2] ) ))) return;
    info->bmiHeader.biSize = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth = size.cx;
    info->bmiHeader.biHeight = size.cy;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biCompression = BI_RGB;

    hdc = CreateCompatibleDC( 0 );
    if (!(dib = CreateDIBSection( 0, info, DIB_RGB_COLORS, &color_bits, NULL, 0 ))) goto done;
    SelectObject( hdc, dib );
    DrawIconEx( hdc, pos.x, pos.y, icon->image, width, height, 0, 0, DI_DEFAULTSIZE | DI_NORMAL );

    /* check if the icon was drawn with an alpha channel */
    for (i = 0, ptr = color_bits; i < size.cx * size.cy; i++)
        if ((has_alpha = (ptr[i] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (size.cx + 31) / 32 * 4;

        info->bmiHeader.biBitCount = 1;
        info->bmiColors[0].rgbRed = 0;
        info->bmiColors[0].rgbGreen = 0;
        info->bmiColors[0].rgbBlue = 0;
        info->bmiColors[0].rgbReserved = 0;
        info->bmiColors[1].rgbRed = 0xff;
        info->bmiColors[1].rgbGreen = 0xff;
        info->bmiColors[1].rgbBlue = 0xff;
        info->bmiColors[1].rgbReserved = 0;

        if (!(mask = CreateDIBSection( 0, info, DIB_RGB_COLORS, &mask_bits, NULL, 0 ))) goto done;
        memset( mask_bits, 0xff, width_bytes * size.cy );
        SelectObject( hdc, mask );
        DrawIconEx( hdc, pos.x, pos.y, icon->image, width, height, 0, 0, DI_DEFAULTSIZE | DI_MASK );

        for (y = 0, ptr = color_bits; y < size.cy; y++)
            for (x = 0; x < size.cx; x++, ptr++)
                if (!((((BYTE *)mask_bits)[y * width_bytes + x / 8] << (x % 8)) & 0x80))
                    *ptr |= 0xff000000;

        SelectObject( hdc, dib );
        DeleteObject( mask );
    }

    UpdateLayeredWindow( icon->window, 0, NULL, NULL, hdc, NULL, 0, &blend, ULW_ALPHA );
done:
    free( info );
    if (hdc) DeleteDC( hdc );
    if (dib) DeleteObject( dib );
}

static BOOL notify_owner( struct icon *icon, UINT msg, LPARAM lparam )
{
    WPARAM wp = icon->id;
    LPARAM lp = msg;

    if (icon->version >= NOTIFYICON_VERSION_4)
    {
        POINT pt = {.x = (short)LOWORD(lparam), .y = (short)HIWORD(lparam)};
        ClientToScreen( icon->window, &pt );
        wp = MAKEWPARAM( pt.x, pt.y );
        lp = MAKELPARAM( msg, icon->id );
    }

    TRACE( "relaying 0x%x\n", msg );
    if (!SendNotifyMessageW( icon->owner, icon->callback_message, wp, lp ) &&
        (GetLastError() == ERROR_INVALID_WINDOW_HANDLE))
    {
        WARN( "application window was destroyed, removing icon %u\n", icon->id );
        delete_icon( icon );
        return FALSE;
    }
    return TRUE;
}

/* window procedure for the individual tray icon window */
static LRESULT WINAPI tray_icon_wndproc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    struct icon *icon = (struct icon *)GetWindowLongPtrW( hwnd, GWLP_USERDATA );

    TRACE( "hwnd %p, msg %#x, wparam %#Ix, lparam %#Ix\n", hwnd, msg, wparam, lparam );

    switch (msg)
    {
    case WM_NCCREATE:
    {
        /* set the icon data for the window from the data passed into CreateWindow */
        const CREATESTRUCTW *info = (const CREATESTRUCTW *)lparam;
        icon = info->lpCreateParams;
        SetWindowLongPtrW( hwnd, GWLP_USERDATA, (LONG_PTR)icon );
        break;
    }

    case WM_CLOSE:
        if (icon->display == ICON_DISPLAY_DOCKED)
        {
            TRACE( "icon %u no longer embedded\n", icon->id );
            hide_icon( icon );
            show_icon( icon );
        }
        return 0;

    case WM_CREATE:
        icon->window = hwnd;
        create_tooltip( icon );
        break;

    case WM_SIZE:
    case WM_MOVE:
        if (icon->layered) paint_layered_icon( icon );
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc;
        int cx, cy;

        if (icon->layered) break;

        cx = GetSystemMetrics( SM_CXSMICON );
        cy = GetSystemMetrics( SM_CYSMICON );
        hdc = BeginPaint( hwnd, &ps );
        GetClientRect( hwnd, &rc );
        TRACE( "painting rect %s\n", wine_dbgstr_rect( &rc ) );
        DrawIconEx( hdc, (rc.left + rc.right - cx) / 2, (rc.top + rc.bottom - cy) / 2,
                    icon->image, cx, cy, 0, 0, DI_DEFAULTSIZE | DI_NORMAL );
        EndPaint( hwnd, &ps );
        return 0;
    }

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDBLCLK:
    {
        MSG message = {.hwnd = hwnd, .message = msg, .wParam = wparam, .lParam = lparam};
        SendMessageW( icon->tooltip, TTM_RELAYEVENT, 0, (LPARAM)&message );
        if (!notify_owner( icon, msg, lparam )) break;
        if (icon->version > 0)
        {
            if (msg == WM_LBUTTONUP) notify_owner( icon, NIN_SELECT, lparam );
            if (msg == WM_RBUTTONUP) notify_owner( icon, WM_CONTEXTMENU, lparam );
        }
        break;
    }

    case WM_WINDOWPOSCHANGING:
        if (icon->display == ICON_DISPLAY_HIDDEN)
        {
            /* Changing the icon's parent via SetParent would activate it, stealing the focus. */
            WINDOWPOS *wp = (WINDOWPOS*)lparam;
            wp->flags |= SWP_NOACTIVATE;
        }
        break;

    case WM_WINDOWPOSCHANGED:
        update_systray_balloon_position();
        break;

    case WM_TIMER:
        switch (wparam)
        {
        case BALLOON_CREATE_TIMER: balloon_create_timer( icon ); break;
        case BALLOON_SHOW_TIMER: balloon_timer( icon ); break;
        }
        return 0;
    }

    return DefWindowProcW( hwnd, msg, wparam, lparam );
}

/* add an icon to the system tray window */
static void systray_add_icon( struct icon *icon )
{
    POINT pos;

    if (icon->display != ICON_DISPLAY_HIDDEN) return;  /* already added */

    SetWindowLongW( icon->window, GWL_STYLE, GetWindowLongW( icon->window, GWL_STYLE ) | WS_CHILD );
    SetParent( icon->window, tray_window );
    icon->display = nb_displayed++;
    pos = get_icon_pos( icon );
    SetWindowPos( icon->window, 0, pos.x, pos.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );

    if (nb_displayed == 1 && show_systray) do_show_systray();
    TRACE( "added %u now %d icons\n", icon->id, nb_displayed );
}

/* remove an icon from the stand-alone tray */
static void systray_remove_icon( struct icon *icon )
{
    struct icon *ptr;
    POINT pos;

    if (icon->display == ICON_DISPLAY_HIDDEN) return;  /* already removed */

    assert( nb_displayed );
    LIST_FOR_EACH_ENTRY( ptr, &icon_list, struct icon, entry )
    {
        if (ptr == icon) continue;
        if (ptr->display < icon->display) continue;
        ptr->display--;
        update_tooltip_position( ptr );
        pos = get_icon_pos( ptr );
        SetWindowPos( ptr->window, 0, pos.x, pos.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER );
    }

    if (!--nb_displayed && !enable_taskbar) do_hide_systray();
    TRACE( "removed %u now %d icons\n", icon->id, nb_displayed );

    icon->display = ICON_DISPLAY_HIDDEN;
    SetParent( icon->window, GetDesktopWindow() );
    SetWindowLongW( icon->window, GWL_STYLE, GetWindowLongW( icon->window, GWL_STYLE ) & ~WS_CHILD );
}

/* make an icon visible */
static void show_icon(struct icon *icon)
{
    TRACE( "id=0x%x, hwnd=%p\n", icon->id, icon->owner );

    if (no_tray_items) return;

    if (icon->display != ICON_DISPLAY_HIDDEN) return;  /* already displayed */

    if (enable_dock)
    {
        DWORD old_exstyle = GetWindowLongW( icon->window, GWL_EXSTYLE );

        /* make sure it is layered before calling into the driver */
        SetWindowLongW( icon->window, GWL_EXSTYLE, old_exstyle | WS_EX_LAYERED );
        paint_layered_icon( icon );

        if (!NtUserMessageCall( icon->window, WINE_SYSTRAY_DOCK_INSERT, icon_cx, icon_cy,
                                icon, NtUserSystemTrayCall, FALSE ))
            SetWindowLongW( icon->window, GWL_EXSTYLE, old_exstyle );
        else
        {
            icon->display = ICON_DISPLAY_DOCKED;
            icon->layered = TRUE;
            SendMessageW( icon->window, WM_SIZE, SIZE_RESTORED, MAKELONG( icon_cx, icon_cy ) );
        }
    }
    systray_add_icon( icon );

    update_tooltip_position( icon );
    update_balloon( icon );
}

/* make an icon invisible */
static void hide_icon(struct icon *icon)
{
    TRACE( "id=0x%x, hwnd=%p\n", icon->id, icon->owner );

    if (icon->display == ICON_DISPLAY_HIDDEN) return;  /* already hidden */

    if (enable_dock && NtUserMessageCall( icon->window, WINE_SYSTRAY_DOCK_REMOVE, 0, 0,
                                          NULL, NtUserSystemTrayCall, FALSE ))
    {
        icon->display = ICON_DISPLAY_HIDDEN;
        icon->layered = FALSE;
        SetWindowLongW( icon->window, GWL_EXSTYLE, GetWindowLongW( icon->window, GWL_EXSTYLE ) & ~WS_EX_LAYERED );
    }
    ShowWindow( icon->window, SW_HIDE );
    systray_remove_icon( icon );

    update_balloon( icon );
    update_tooltip_position( icon );
}

/* Modifies an existing icon record */
static BOOL modify_icon( struct icon *icon, NOTIFYICONDATAW *nid )
{
    TRACE( "id=0x%x, hwnd=%p\n", nid->uID, nid->hWnd );

    /* demarshal the request from the NID */
    if (!icon)
    {
        WARN( "Invalid icon ID (0x%x) for HWND %p\n", nid->uID, nid->hWnd );
        return FALSE;
    }

    if (nid->uFlags & NIF_STATE)
    {
        icon->state = (icon->state & ~nid->dwStateMask) | (nid->dwState & nid->dwStateMask);
    }

    if (nid->uFlags & NIF_ICON)
    {
        if (icon->image) DestroyIcon(icon->image);
        icon->image = CopyIcon(nid->hIcon);

        if (icon->display >= 0)
            InvalidateRect( icon->window, NULL, TRUE );
        else if (icon->layered)
            paint_layered_icon( icon );
        else if (enable_dock)
            NtUserMessageCall( icon->window, WINE_SYSTRAY_DOCK_CLEAR, 0, 0,
                               NULL, NtUserSystemTrayCall, FALSE );
    }

    if (nid->uFlags & NIF_MESSAGE)
    {
        icon->callback_message = nid->uCallbackMessage;
    }
    if (nid->uFlags & NIF_TIP)
    {
        lstrcpynW( icon->tiptext, nid->szTip, ARRAY_SIZE( icon->tiptext ));
        update_tooltip_text( icon );
    }
    if (nid->uFlags & NIF_INFO && nid->cbSize >= NOTIFYICONDATAA_V2_SIZE)
    {
        lstrcpynW( icon->info_text, nid->szInfo, ARRAY_SIZE( icon->info_text ));
        lstrcpynW( icon->info_title, nid->szInfoTitle, ARRAY_SIZE( icon->info_title ));
        icon->info_flags = nid->dwInfoFlags;
        icon->info_timeout = max(min(nid->uTimeout, BALLOON_SHOW_MAX_TIMEOUT), BALLOON_SHOW_MIN_TIMEOUT);

        if (icon->info_icon) DestroyIcon( icon->info_icon );
        icon->info_icon = CopyIcon( nid->hBalloonIcon );

        update_balloon( icon );
    }
    if (icon->state & NIS_HIDDEN) hide_icon( icon );
    else show_icon( icon );
    return TRUE;
}

/* Adds a new icon record to the list */
static BOOL add_icon(NOTIFYICONDATAW *nid)
{
    struct icon  *icon;

    TRACE( "id=0x%x, hwnd=%p\n", nid->uID, nid->hWnd );

    if ((icon = get_icon(nid->hWnd, nid->uID)))
    {
        WARN( "duplicate tray icon add, buggy app?\n" );
        return FALSE;
    }

    if (!(icon = calloc( 1, sizeof(*icon) )))
    {
        ERR( "out of memory\n" );
        return FALSE;
    }

    ZeroMemory(icon, sizeof(struct icon));
    icon->id     = nid->uID;
    icon->owner  = nid->hWnd;
    icon->display = ICON_DISPLAY_HIDDEN;

    CreateWindowExW( 0, tray_icon_class.lpszClassName, NULL, WS_CLIPSIBLINGS | WS_POPUP,
                     0, 0, icon_cx, icon_cy, 0, NULL, NULL, icon );
    if (!icon->window) ERR( "Failed to create systray icon window\n" );

    list_add_tail(&icon_list, &icon->entry);

    return modify_icon( icon, nid );
}

/* Deletes tray icon window and icon record */
static BOOL delete_icon( struct icon *icon )
{
    hide_icon( icon );
    list_remove( &icon->entry );
    DestroyWindow( icon->tooltip );
    DestroyWindow( icon->window );
    DestroyIcon( icon->image );
    free( icon );
    return TRUE;
}

/* cleanup icons belonging to a window that has been destroyed */
static void cleanup_systray_window( HWND hwnd )
{
    NOTIFYICONDATAW nid = {.cbSize = sizeof(nid), .hWnd = hwnd};
    struct icon *icon, *next;

    LIST_FOR_EACH_ENTRY_SAFE( icon, next, &icon_list, struct icon, entry )
        if (icon->owner == hwnd) delete_icon( icon );

    NtUserMessageCall( hwnd, WINE_SYSTRAY_CLEANUP_ICONS, 0, 0, NULL, NtUserSystemTrayCall, FALSE );
}

/* update the taskbar buttons when something changed */
static void sync_taskbar_buttons(void)
{
    struct taskbar_button *win;
    BOOL vertical = taskbar_edge == ABE_LEFT || taskbar_edge == ABE_RIGHT;
    int pos = 0, count = 0;
    int width = quicklaunch_mode() == 2 ? 48 :
                vertical ? taskbar_button_height : taskbar_button_width;
    int right = (vertical ? tray_height - clock_height : tray_width - clock_width) -
                nb_displayed * (vertical ? icon_cy : icon_cx) - inputlang_extent(vertical);
    HWND foreground = GetAncestor( GetForegroundWindow(), GA_ROOTOWNER );

    if (!enable_taskbar || !IsWindowVisible( tray_window )) return;

    /* Both the language control and icons occupy the notification strip.
     * Reserve their width/height before assigning space to app buttons. */
    inputlang_layout(tray_width, tray_height, clock_width, clock_height,
                     icon_cx, icon_cy, nb_displayed, taskbar_edge);
    quicklaunch_reset_running();

    LIST_FOR_EACH_ENTRY( win, &taskbar_buttons, struct taskbar_button, entry )
    {
        if (!win->hwnd)
        {
            int length = vertical ? start_button_height : start_button_width;
            SetWindowPos( win->button, 0, 0, 0, vertical ? tray_width : length,
                          vertical ? length : tray_height,
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW );
            pos += length;
            continue;
        }
        win->active = (win->hwnd == foreground);
        win->visible = IsWindowVisible( win->hwnd ) && !GetWindow( win->hwnd, GW_OWNER );
        if (win->visible && quicklaunch_bind_window(win->hwnd, win->executable, win->active))
            win->visible = FALSE; /* The pinned button represents this window. */
        if (win->visible) count++;
    }

    /* Old Quick Launch remains separate; Windows 7+ pinned icons use
     * these same slots after running-window reconciliation. */
    pos = quicklaunch_layout(pos, right, vertical, tray_width, tray_height);

    if (count && count * width > right - pos)
        width = max( vertical ? taskbar_button_height / 2 : taskbar_button_width / 4,
                     (right - pos) / count );

    LIST_FOR_EACH_ENTRY( win, &taskbar_buttons, struct taskbar_button, entry )
    {
        if (!win->hwnd) continue;
        if (win->visible && right - pos >= width)
        {
            SetWindowPos( win->button, 0, vertical ? 0 : pos, vertical ? pos : 0,
                          vertical ? tray_width : width, vertical ? width : tray_height,
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW );
            InvalidateRect( win->button, NULL, TRUE );
            pos += width;
        }
        else SetWindowPos( win->button, 0, 0, 0, 0, 0,
                           SWP_NOZORDER | SWP_NOACTIVATE | SWP_HIDEWINDOW );
    }
}

static BOOL handle_incoming(HWND hwndSource, COPYDATASTRUCT *cds)
{
    struct icon *icon = NULL;
    const struct notify_data *data;
    const BYTE *icon_data;
    NOTIFYICONDATAW nid;
    int ret = FALSE;

    if (cds->cbData < sizeof(*data)) return FALSE;
    data = cds->lpData;
    icon_data = data->icon_data;

    nid.cbSize           = sizeof(nid);
    nid.hWnd             = LongToHandle( data->hWnd );
    nid.uID              = data->uID;
    nid.uFlags           = data->uFlags;
    nid.uCallbackMessage = data->uCallbackMessage;
    nid.hIcon            = 0;
    nid.dwState          = data->dwState;
    nid.dwStateMask      = data->dwStateMask;
    nid.uTimeout         = data->u.uTimeout;
    nid.dwInfoFlags      = data->dwInfoFlags;
    nid.guidItem         = data->guidItem;
    lstrcpyW( nid.szTip, data->szTip );
    lstrcpyW( nid.szInfo, data->szInfo );
    lstrcpyW( nid.szInfoTitle, data->szInfoTitle );
    nid.hBalloonIcon     = 0;

    /* FIXME: if statement only needed because we don't support interprocess
     * icon handles */
    if (nid.uFlags & NIF_ICON)
    {
        LONG cbMaskBits;
        LONG cbColourBits;

        cbMaskBits = (data->icon_info.width * data->icon_info.height + 15) / 16 * 2;
        cbColourBits = (data->icon_info.planes * data->icon_info.width * data->icon_info.height * data->icon_info.bpp + 15) / 16 * 2;

        if (cds->cbData < sizeof(*data) + cbMaskBits + cbColourBits)
        {
            ERR( "buffer underflow\n" );
            return FALSE;
        }
        nid.hIcon = CreateIcon(NULL, data->icon_info.width, data->icon_info.height, data->icon_info.planes, data->icon_info.bpp,
                               icon_data, icon_data + cbMaskBits);
        icon_data += cbMaskBits + cbColourBits;
    }

    if ((nid.uFlags & NIF_INFO) && (nid.dwInfoFlags & NIIF_ICONMASK) == NIIF_USER)
    {
        /* Balloon icon */
        LONG cbMaskBits;
        LONG cbColourBits;

        cbMaskBits = (data->balloon_icon_info.width * data->balloon_icon_info.height + 15) / 16 * 2;
        cbColourBits = (data->balloon_icon_info.planes * data->balloon_icon_info.width * data->balloon_icon_info.height * data->balloon_icon_info.bpp + 15) / 16 * 2;

        if (cds->cbData < ((char*)icon_data - (char*)data) + cbMaskBits + cbColourBits)
        {
            ERR( "buffer underflow\n" );
            return FALSE;
        }
        nid.hBalloonIcon = CreateIcon(NULL, data->balloon_icon_info.width, data->balloon_icon_info.height, data->balloon_icon_info.planes, data->balloon_icon_info.bpp,
                                      icon_data, icon_data + cbMaskBits);
    }
    /* try forwarding to the display driver first */
    if (cds->dwData == NIM_ADD || !(icon = get_icon( nid.hWnd, nid.uID )))
    {
        if ((ret = NtUserMessageCall( hwndSource, WINE_SYSTRAY_NOTIFY_ICON, cds->dwData, 0,
                                      &nid, NtUserSystemTrayCall, FALSE )) != -1)
            goto done;
        ret = FALSE;
    }

    switch (cds->dwData)
    {
    case NIM_ADD:
        ret = add_icon(&nid);
        break;
    case NIM_DELETE:
        if (icon) ret = delete_icon( icon );
        break;
    case NIM_MODIFY:
        if (icon) ret = modify_icon( icon, &nid );
        break;
    case NIM_SETVERSION:
        if (icon)
        {
            icon->version = nid.uVersion;
            ret = TRUE;
        }
        break;
    default:
        FIXME( "unhandled tray message: %Id\n", cds->dwData );
        break;
    }

done:
    if (nid.hIcon) DestroyIcon( nid.hIcon );
    if (nid.hBalloonIcon) DestroyIcon( nid.hBalloonIcon );
    sync_taskbar_buttons();
    return ret;
}

static void add_taskbar_button( HWND hwnd )
{
    struct taskbar_button *win;

    if (!enable_taskbar) return;

    /* ignore our own windows */
    if (hwnd)
    {
        DWORD process;
        if (!GetWindowThreadProcessId( hwnd, &process ) || process == GetCurrentProcessId()) return;
    }

    if (!(win = calloc(1, sizeof(*win)))) return;
    win->hwnd = hwnd;
    if (hwnd && quicklaunch_mode() == 2)
    {
        DWORD process_id;
        HANDLE process;
        DWORD length = ARRAY_SIZE(win->executable);

        GetWindowThreadProcessId(hwnd, &process_id);
        process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
        if (process)
        {
            if (!QueryFullProcessImageNameW(process, 0, win->executable, &length))
                win->executable[0] = 0;
            CloseHandle(process);
        }
    }
    win->button = CreateWindowW( WC_BUTTONW, NULL, WS_CHILD | BS_OWNERDRAW,
                                 0, 0, 0, 0, tray_window, (HMENU)hwnd, 0, 0 );
    list_add_tail( &taskbar_buttons, &win->entry );
}

static struct taskbar_button *find_taskbar_button( HWND hwnd )
{
    struct taskbar_button *win;

    LIST_FOR_EACH_ENTRY( win, &taskbar_buttons, struct taskbar_button, entry )
        if (win->hwnd == hwnd) return win;

    return NULL;
}

static void remove_taskbar_button( HWND hwnd )
{
    struct taskbar_button *win = find_taskbar_button( hwnd );

    if (!win) return;
    list_remove( &win->entry );
    DestroyWindow( win->button );
    free( win );
}

/* Default visual identities only. Custom OS theme selection and native DWM
 * transparency are separate capabilities; Vista's palette is opaque for now.
 * Require explicit NT identity: never map the Win9x fallback to XP/Vista. */
static enum taskbar_visual_style get_default_taskbar_visual(void)
{
    char line[32], version[32];
    DWORD len, build;
    WORD major, minor;

    len = GetEnvironmentVariableA( WATER_WIN16_LINE_ENV, line, sizeof(line) );
    if (!len || len >= sizeof(line) || strcmp( line, WATER_WIN16_LINE_NT ))
        return TASKBAR_VISUAL_CLASSIC;

    len = GetEnvironmentVariableA( WATER_WIN16_VERSION_ENV, version, sizeof(version) );
    if (!len || len >= sizeof(version) ||
        !water_win16_parse_version( version, &major, &minor, &build ))
        return TASKBAR_VISUAL_CLASSIC;

    if (major == 5 && minor == 1) return TASKBAR_VISUAL_LUNA_BLUE;
    if (major == 6 && minor == 0) return TASKBAR_VISUAL_VISTA_AERO;
    return TASKBAR_VISUAL_CLASSIC;
}

static COLORREF taskbar_color_mix( COLORREF first, COLORREF second, int index, int count )
{
    if (!count) return first;
    return RGB( (GetRValue(first) * (count - index) + GetRValue(second) * index) / count,
                (GetGValue(first) * (count - index) + GetGValue(second) * index) / count,
                (GetBValue(first) * (count - index) + GetBValue(second) * index) / count );
}

/* Paint along taskbar thickness, which is horizontal for side docking. */
static void paint_taskbar_gradient( HDC dc, RECT rect, COLORREF top, COLORREF bottom,
                                    BOOL side_docked )
{
    int thickness = side_docked ? rect.right - rect.left : rect.bottom - rect.top;
    int bands = min( 32, max( 1, thickness ) );
    int index;

    for (index = 0; index < bands; index++)
    {
        RECT band = rect;
        HBRUSH brush;
        COLORREF color = taskbar_color_mix( top, bottom, index, max( 1, bands - 1 ) );

        if (side_docked)
        {
            band.left = rect.left + thickness * index / bands;
            band.right = rect.left + thickness * (index + 1) / bands;
        }
        else
        {
            band.top = rect.top + thickness * index / bands;
            band.bottom = rect.top + thickness * (index + 1) / bands;
        }
        brush = CreateSolidBrush( color );
        if (brush)
        {
            FillRect( dc, &band, brush );
            DeleteObject( brush );
        }
    }
}

static void paint_themed_taskbar( HWND hwnd, HDC dc )
{
    RECT rect;

    GetClientRect( hwnd, &rect );
    if (taskbar_visual == TASKBAR_VISUAL_LUNA_BLUE)
        paint_taskbar_gradient( dc, rect, RGB(74, 147, 241), RGB(24, 75, 179),
                                taskbar_edge == ABE_LEFT || taskbar_edge == ABE_RIGHT );
    else if (taskbar_visual == TASKBAR_VISUAL_VISTA_AERO)
        paint_taskbar_gradient( dc, rect, RGB(84, 87, 95), RGB(22, 24, 29),
                                taskbar_edge == ABE_LEFT || taskbar_edge == ABE_RIGHT );
}

static void paint_themed_taskbar_button( const DRAWITEMSTRUCT *dis,
                                         const struct taskbar_button *win, RECT rect )
{
    COLORREF top, bottom;
    BOOL start = !win->hwnd;
    BOOL pressed = !!(dis->itemState & ODS_SELECTED) || win->active;
    BOOL side_docked = taskbar_edge == ABE_LEFT || taskbar_edge == ABE_RIGHT;
    HFONT old_font;
    WCHAR title[256] = {0};

    if (taskbar_visual == TASKBAR_VISUAL_LUNA_BLUE)
    {
        top = start ? RGB(101, 199, 69) :
              pressed ? RGB(22, 65, 150) : RGB(73, 138, 233);
        bottom = start ? RGB(30, 122, 30) :
                 pressed ? RGB(28, 88, 178) : RGB(38, 94, 197);
    }
    else
    {
        top = start ? RGB(43, 47, 54) :
              pressed ? RGB(75, 85, 104) : RGB(88, 93, 103);
        bottom = start ? RGB(17, 20, 25) :
                 pressed ? RGB(38, 49, 67) : RGB(34, 38, 46);
    }

    paint_taskbar_gradient( dis->hDC, rect, top, bottom, side_docked );
    SetBkMode( dis->hDC, TRANSPARENT );
    SetTextColor( dis->hDC, RGB(255, 255, 255) );
    old_font = SelectObject( dis->hDC, GetStockObject( DEFAULT_GUI_FONT ) );

    if (start && taskbar_visual == TASKBAR_VISUAL_VISTA_AERO)
    {
        HBRUSH circle = CreateSolidBrush( RGB(23, 135, 208) );
        HBRUSH previous = SelectObject( dis->hDC, circle );
        RECT badge = rect;
        int size = min( 30, min( rect.right - rect.left - 4, rect.bottom - rect.top - 4 ) );

        badge.left = (rect.right - size) / 2;
        badge.top = (rect.bottom - size) / 2;
        badge.right = badge.left + size;
        badge.bottom = badge.top + size;
        Ellipse( dis->hDC, badge.left, badge.top, badge.right, badge.bottom );
        SelectObject( dis->hDC, previous );
        DeleteObject( circle );
        DrawIconEx( dis->hDC, badge.left + (size - 18) / 2,
                    badge.top + (size - 18) / 2, shell_traywnd_class.hIcon,
                    18, 18, 0, NULL, DI_NORMAL );
    }
    else
    {
        if (start) lstrcpynW( title, start_label, ARRAY_SIZE(title) );
        else
        {
            HICON icon = (HICON)GetClassLongPtrW(win->hwnd, GCLP_HICONSM);
            if (!icon) icon = (HICON)GetClassLongPtrW(win->hwnd, GCLP_HICON);
            if (!icon) icon = LoadIconW(NULL, IDI_APPLICATION);
            DrawIconEx(dis->hDC, rect.left + 6, (rect.bottom - 16) / 2,
                       icon, 16, 16, 0, NULL, DI_NORMAL);
            rect.left += 22;
            GetWindowTextW( win->hwnd, title, ARRAY_SIZE(title) );
        }
        rect.left += start ? 8 : 10;
        rect.right -= 6;
        shell_draw_text( dis->hDC, title, &rect, DT_SINGLELINE | DT_VCENTER |
                         (start ? DT_CENTER : DT_LEFT) | DT_END_ELLIPSIS,
                         start ? GetUserDefaultUILanguage() :
                         LOWORD((ULONG_PTR)GetKeyboardLayout(
                             GetWindowThreadProcessId(win->hwnd, NULL))) );
    }
    SelectObject( dis->hDC, old_font );
}

static void paint_unified_window_button(const DRAWITEMSTRUCT *dis,
                                        const struct taskbar_button *win, RECT rc)
{
    COLORREF background = win->active ? RGB(140, 171, 211) : GetSysColor(COLOR_BTNFACE);
    HBRUSH brush = CreateSolidBrush(background);
    HICON icon = (HICON)GetClassLongPtrW(win->hwnd, GCLP_HICONSM);

    if (brush)
    {
        FillRect(dis->hDC, &rc, brush);
        DeleteObject(brush);
    }
    if (!icon) icon = (HICON)GetClassLongPtrW(win->hwnd, GCLP_HICON);
    if (!icon) icon = LoadIconW(NULL, IDI_APPLICATION);
    DrawIconEx(dis->hDC, (rc.right - 32) / 2, (rc.bottom - 32) / 2,
               icon, 32, 32, 0, NULL, DI_NORMAL);
    rc.top = rc.bottom - 3;
    brush = CreateSolidBrush(win->active ? RGB(172, 219, 255) : RGB(104, 159, 207));
    if (brush)
    {
        FillRect(dis->hDC, &rc, brush);
        DeleteObject(brush);
    }
}

/* Use real Win9x ANSI messages and GDI, not GetWindowTextW or
 * DrawCaptionTempW. This retains DBCS byte pairs in the active ACP. */
static void paint_win9x_taskbar_caption(const DRAWITEMSTRUCT *dis,
                                       const struct taskbar_button *win, RECT rect)
{
    char caption[512] = {0};
    RECT text_rect = rect;
    HICON icon;
    HGDIOBJ previous;

    if (win->hwnd)
    {
        GetWindowTextA(win->hwnd, caption, ARRAY_SIZE(caption));
        icon = (HICON)GetClassLongPtrA(win->hwnd, GCLP_HICONSM);
        if (!icon) icon = (HICON)GetClassLongPtrA(win->hwnd, GCLP_HICON);
    }
    else
    {
        WideCharToMultiByte(CP_ACP, 0, start_label, -1, caption,
                            ARRAY_SIZE(caption), NULL, NULL);
        icon = shell_traywnd_class.hIcon;
    }
    if (!icon) icon = LoadIconA(NULL, IDI_APPLICATION);

    DrawIconEx(dis->hDC, rect.left + 5, (rect.bottom - 16) / 2,
               icon, 16, 16, 0, NULL, DI_NORMAL);
    text_rect.left += 25;
    text_rect.right -= 6;
    previous = SelectObject(dis->hDC, GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(dis->hDC, TRANSPARENT);
    SetTextColor(dis->hDC, GetSysColor(COLOR_BTNTEXT));
    DrawTextA(dis->hDC, caption, -1, &text_rect,
              DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    if (previous && previous != HGDI_ERROR) SelectObject(dis->hDC, previous);
}

static void paint_taskbar_button( const DRAWITEMSTRUCT *dis )
{
    RECT rect;
    UINT flags = DC_TEXT;
    struct taskbar_button *win = find_taskbar_button( LongToHandle( dis->CtlID ));

    if (!win) return;
    GetClientRect( dis->hwndItem, &rect );
    if (!win->hwnd && quicklaunch_mode() == 2)
    {
        /* Windows 7+ Start is an icon, not a labeled task button. */
        HBRUSH brush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
        if (brush)
        {
            FillRect(dis->hDC, &rect, brush);
            DeleteObject(brush);
        }
        DrawIconEx(dis->hDC, (rect.right - 28) / 2, (rect.bottom - 28) / 2,
                   shell_traywnd_class.hIcon, 28, 28, 0, NULL, DI_NORMAL);
        return;
    }
    if (win->hwnd && quicklaunch_mode() == 2)
    {
        paint_unified_window_button(dis, win, rect);
        return;
    }
    if (taskbar_visual != TASKBAR_VISUAL_CLASSIC)
    {
        paint_themed_taskbar_button( dis, win, rect );
        return;
    }
    DrawFrameControl( dis->hDC, &rect, DFC_BUTTON, DFCS_BUTTONPUSH | DFCS_ADJUSTRECT |
                      ((dis->itemState & ODS_SELECTED) ? DFCS_PUSHED : 0 ));
    if (shell_text_is_win9x())
    {
        paint_win9x_taskbar_caption(dis, win, rect);
        return;
    }
    if (win->hwnd)
    {
        WCHAR title[256];
        UINT thread;
        LANGID locale;

        GetWindowTextW(win->hwnd, title, ARRAY_SIZE(title));
        if (shell_text_needs_cjk(title))
        {
            HICON icon = (HICON)GetClassLongPtrW(win->hwnd, GCLP_HICONSM);
            HFONT old_font = SelectObject(dis->hDC, GetStockObject(DEFAULT_GUI_FONT));
            RECT caption = rect;

            if (!icon) icon = (HICON)GetClassLongPtrW(win->hwnd, GCLP_HICON);
            if (!icon) icon = LoadIconW(NULL, IDI_APPLICATION);
            DrawIconEx(dis->hDC, rect.left + 5, (rect.bottom - 16) / 2,
                       icon, 16, 16, 0, NULL, DI_NORMAL);
            caption.left += 26;
            caption.right -= 5;
            SetBkMode(dis->hDC, TRANSPARENT);
            SetTextColor(dis->hDC, GetSysColor(COLOR_BTNTEXT));
            thread = GetWindowThreadProcessId(win->hwnd, NULL);
            locale = thread ? LOWORD((ULONG_PTR)GetKeyboardLayout(thread)) :
                              GetUserDefaultUILanguage();
            shell_draw_text(dis->hDC, title, &caption,
                            DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS,
                            locale);
            SelectObject(dis->hDC, old_font);
        }
        else
        {
            flags |= win->active ? DC_ACTIVE : DC_INBUTTON;
            DrawCaptionTempW(win->hwnd, dis->hDC, &rect, 0, 0, NULL, flags);
        }
    }
    else if (start_screen_version() == 3)  /* Windows 8.1 Start button */
        DrawIconEx(dis->hDC, (rect.right - 24) / 2, (rect.bottom - 24) / 2,
                   shell_traywnd_class.hIcon, 24, 24, 0, NULL, DI_NORMAL);
    else  /* classic Start button */
        DrawCaptionTempW( 0, dis->hDC, &rect, 0, 0, start_label, flags | DC_INBUTTON | DC_ICON );
}

static void click_taskbar_button( HWND button )
{
    LONG_PTR id = GetWindowLongPtrW( button, GWLP_ID );
    HWND hwnd = (HWND)id;

    if (!hwnd)  /* start button */
    {
        do_startmenu( tray_window );
        return;
    }

    if (IsIconic( hwnd ))
    {
        SendMessageW( hwnd, WM_SYSCOMMAND, SC_RESTORE, 0 );
        return;
    }

    if (IsWindowEnabled( hwnd ))
    {
        if (hwnd == GetForegroundWindow())
        {
            SendMessageW( hwnd, WM_SYSCOMMAND, SC_MINIMIZE, 0 );
            return;
        }
    }
    else  /* look for an enabled window owned by this one */
    {
        HWND owned = GetWindow( GetDesktopWindow(), GW_CHILD );
        while (owned && owned != hwnd)
        {
            if (IsWindowVisible( owned ) &&
                IsWindowEnabled( owned ) &&
                (GetWindow( owned, GW_OWNER ) == hwnd))
                break;
            owned = GetWindow( owned, GW_HWNDNEXT );
        }
        hwnd = owned;
    }
    SetForegroundWindow( hwnd );
}

static void show_taskbar_contextmenu( HWND button, LPARAM lparam )
{
    ULONG_PTR id = GetWindowLongPtrW( button, GWLP_ID );

    if (id) SendNotifyMessageW( (HWND)id, WM_POPUPSYSTEMMENU, 0, lparam );
}

/* Expose only the docked taskbar, never the standalone notification window. */
BOOL get_taskbar_info( RECT *rect, UINT *state, UINT *edge )
{
    if (!enable_taskbar || !tray_window || !IsWindow( tray_window )) return FALSE;
    if (rect && !GetWindowRect( tray_window, rect )) return FALSE;
    if (edge) *edge = taskbar_edge;
    if (state)
        *state = (GetWindowLongW( tray_window, GWL_EXSTYLE ) & WS_EX_TOPMOST) ? ABS_ALWAYSONTOP : 0;
    /* Autohide is not implemented; do not advertise ABS_AUTOHIDE. */
    return TRUE;
}

/* Windows taskbar GPO DWORDs live under the Policies\Explorer key,
 * independently of the user's selected taskbar settings. Never mirror an
 * administrator's forced value into HKCU\Software\Wine\Explorer\Taskbar. */
static BOOL taskbar_policy_enabled( const WCHAR *name )
{
    static const WCHAR key[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    DWORD value, size;
    HKEY roots[] = { HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE };
    UINT i;

    for (i = 0; i < ARRAY_SIZE(roots); ++i)
    {
        size = sizeof(value);
        if (!RegGetValueW(roots[i], key, name, RRF_RT_REG_DWORD,
                          NULL, &value, &size) && value)
            return TRUE;
    }
    return FALSE;
}

static BOOL taskbar_clock_visible(void)
{
    return taskbar_show_clock && !taskbar_policy_enabled(L"HideClock");
}

static BOOL taskbar_is_locked(void)
{
    return taskbar_locked || taskbar_policy_enabled(L"LockTaskbar");
}

static BOOL read_taskbar_show_clock(void)
{
    DWORD value, size = sizeof(value);

    if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                      L"ShowClock", RRF_RT_REG_DWORD, NULL, &value, &size))
        return !!value;
    return TRUE; /* Existing installations keep their historical default. */
}

static void write_taskbar_show_clock(void)
{
    HKEY key;
    DWORD value = taskbar_show_clock;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                        0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL))
    {
        WARN("Could not open taskbar settings to save clock visibility\n");
        return;
    }
    if (RegSetValueExW(key, L"ShowClock", 0, REG_DWORD,
                       (const BYTE *)&value, sizeof(value)))
        WARN("Could not save taskbar clock visibility\n");
    RegCloseKey(key);
}

static void update_taskbar_clock(void)
{
    if (!clock_window || !taskbar_clock_visible()) return;

    if (shell_text_is_win9x())
    {
        char time[64];

        if (GetTimeFormatA(LOCALE_USER_DEFAULT, TIME_NOSECONDS, NULL, NULL,
                           time, ARRAY_SIZE(time)))
            SetWindowTextA(clock_window, time);
    }
    else
    {
        WCHAR time[64];

        if (GetTimeFormatW(LOCALE_USER_DEFAULT, TIME_NOSECONDS, NULL, NULL,
                           time, ARRAY_SIZE(time)))
            SetWindowTextW(clock_window, time);
    }
}

static void read_taskbar_settings(void)
{
    DWORD value, size = sizeof(value);

    taskbar_show_clock = read_taskbar_show_clock();

    if (!RegGetValueW( HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                       L"Edge", RRF_RT_REG_DWORD, NULL, &value, &size ) &&
        value <= ABE_BOTTOM)
        taskbar_edge = value;

    size = sizeof(value);
    if (!RegGetValueW( HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                       L"Locked", RRF_RT_REG_DWORD, NULL, &value, &size ))
        taskbar_locked = !!value;
}

static void write_taskbar_settings(void)
{
    HKEY key;
    DWORD edge = taskbar_edge, locked = taskbar_locked;

    if (RegCreateKeyExW( HKEY_CURRENT_USER, L"Software\\Wine\\Explorer\\Taskbar",
                         0, NULL, 0, KEY_SET_VALUE, NULL, &key, NULL ))
    {
        WARN( "Could not save taskbar settings\n" );
        return;
    }
    RegSetValueExW( key, L"Edge", 0, REG_DWORD, (const BYTE *)&edge, sizeof(edge) );
    RegSetValueExW( key, L"Locked", 0, REG_DWORD, (const BYTE *)&locked, sizeof(locked) );
    RegCloseKey( key );
}

/* Reserve primary monitor workspace for the current taskbar edge. */
static void update_taskbar_workarea(void)
{
    RECT area, current;

    if (!enable_taskbar) return;

    SetRect( &area, 0, 0, GetSystemMetrics( SM_CXSCREEN ), GetSystemMetrics( SM_CYSCREEN ) );
    switch (taskbar_edge)
    {
    case ABE_LEFT:   area.left += tray_width; break;
    case ABE_TOP:    area.top += tray_height; break;
    case ABE_RIGHT:  area.right -= tray_width; break;
    case ABE_BOTTOM: area.bottom -= tray_height; break;
    }
    if (SystemParametersInfoW( SPI_GETWORKAREA, 0, &current, 0 ) && EqualRect( &current, &area ))
        return;
    if (!SystemParametersInfoW( SPI_SETWORKAREA, 0, &area, SPIF_SENDCHANGE ))
        WARN( "Could not update taskbar work area\n" );
}

static UINT closest_taskbar_edge( POINT point )
{
    const int right = GetSystemMetrics( SM_CXSCREEN );
    const int bottom = GetSystemMetrics( SM_CYSCREEN );
    int distances[4], best;
    UINT edge;

    point.x = max( 0, min( point.x, right ) );
    point.y = max( 0, min( point.y, bottom ) );
    distances[ABE_LEFT] = point.x;
    distances[ABE_TOP] = point.y;
    distances[ABE_RIGHT] = right - point.x;
    distances[ABE_BOTTOM] = bottom - point.y;
    best = distances[taskbar_edge];
    edge = taskbar_edge;
    for (UINT i = ABE_LEFT; i <= ABE_BOTTOM; i++)
    {
        if (distances[i] < best)
        {
            best = distances[i];
            edge = i;
        }
    }
    return edge;
}

static void move_taskbar_from_cursor(void)
{
    POINT point;
    UINT edge;

    if (!taskbar_dragging || !GetCursorPos( &point )) return;
    if (abs( point.x - taskbar_drag_start.x ) < GetSystemMetrics( SM_CXDRAG ) &&
        abs( point.y - taskbar_drag_start.y ) < GetSystemMetrics( SM_CYDRAG ))
        return;

    edge = closest_taskbar_edge( point );
    if (edge == taskbar_edge) return;
    taskbar_edge = edge;
    do_show_systray();
    notify_appbar_taskbar_moved();
}

static void show_taskbar_menu( HWND hwnd )
{
    WCHAR label[64];
    HMENU menu = CreatePopupMenu();
    POINT cursor;
    UINT command;

    if (!menu) return;
    if (taskbar_policy_enabled(L"NoTrayContextMenu"))
    {
        DestroyMenu(menu);
        return;
    }
    if (!LoadStringW( NULL, IDS_TASKBAR_LOCK, label, ARRAY_SIZE(label) ))
        lstrcpyW( label, L"Lock the Taskbar" );
    AppendMenuW( menu, MF_STRING |
                 (taskbar_is_locked() ? MF_CHECKED : MF_UNCHECKED) |
                 (taskbar_policy_enabled(L"LockTaskbar") ? MF_GRAYED : 0),
                 TASKBAR_LOCK_COMMAND, label );
    if (!LoadStringW(NULL, IDS_TASKBAR_SHOW_CLOCK, label, ARRAY_SIZE(label)))
        lstrcpyW(label, L"Show the Clock");
    AppendMenuW(menu, MF_STRING |
                (taskbar_clock_visible() ? MF_CHECKED : MF_UNCHECKED) |
                (taskbar_policy_enabled(L"HideClock") ? MF_GRAYED : 0),
                TASKBAR_CLOCK_COMMAND, label);
    if (quicklaunch_can_toggle())
    {
        AppendMenuW(menu, MF_STRING |
                    (quicklaunch_is_enabled() ? MF_CHECKED : MF_UNCHECKED),
                    TASKBAR_QUICKLAUNCH_COMMAND, L"Show Quick Launch");
    }
    GetCursorPos( &cursor );
    command = TrackPopupMenu( menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                              cursor.x, cursor.y, 0, hwnd, NULL );
    DestroyMenu( menu );
    if (command == TASKBAR_LOCK_COMMAND && !taskbar_policy_enabled(L"LockTaskbar"))
    {
        taskbar_locked = !taskbar_locked;
        write_taskbar_settings();
    }
    else if (command == TASKBAR_CLOCK_COMMAND && !taskbar_policy_enabled(L"HideClock"))
    {
        taskbar_show_clock = !taskbar_show_clock;
        write_taskbar_show_clock();
        do_show_systray(); /* Reclaim or reserve the clock's taskbar slot. */
    }
    else if (command == TASKBAR_QUICKLAUNCH_COMMAND && quicklaunch_can_toggle())
    {
        quicklaunch_set_enabled(!quicklaunch_is_enabled());
        quicklaunch_initialize(tray_window, GetSysColor(COLOR_BTNFACE));
        sync_taskbar_buttons();
    }
}

static void do_hide_systray(void)
{
    ShowWindow( tray_window, SW_HIDE );
}

static void do_show_systray(void)
{
    SIZE size;
    NONCLIENTMETRICSW ncm;
    HFONT font;
    HDC hdc;

    if (!enable_taskbar)
    {
        size = get_window_size();
        SetWindowPos( tray_window, 0, 0, 0, size.cx, size.cy, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW );
        return;
    }

    hdc = GetDC( 0 );

    ncm.cbSize = sizeof(NONCLIENTMETRICSW);
    SystemParametersInfoW( SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICSW), &ncm, 0 );
    font = CreateFontIndirectW( &ncm.lfCaptionFont );
    /* FIXME: Implement BCM_GETIDEALSIZE and use that instead. */
    {
        HFONT old_font = SelectObject( hdc, font );
        WCHAR label[64];
        SYSTEMTIME sample = {0};
        SIZE clock_size = {0};

        GetTextExtentPointW( hdc, L"abcdefghijklmnopqrstuvwxyz", 26, &size );
        taskbar_button_width = size.cx;
        GetTextExtentPointW( hdc, start_label, lstrlenW(start_label), &size );

        clock_width = 0;
        if (clock_window)
            ShowWindow(clock_window, taskbar_clock_visible() ? SW_SHOWNOACTIVATE : SW_HIDE);
        if (clock_window && taskbar_clock_visible())
        {
            sample.wHour = 23;
            sample.wMinute = 59;
            SelectObject( hdc, GetStockObject( DEFAULT_GUI_FONT ));
            if (shell_text_is_win9x())
            {
                char labelA[64];

                if (GetTimeFormatA(LOCALE_USER_DEFAULT, TIME_NOSECONDS, &sample, NULL,
                                   labelA, ARRAY_SIZE(labelA)))
                    GetTextExtentPointA(hdc, labelA, lstrlenA(labelA), &clock_size);
            }
            else if (GetTimeFormatW( LOCALE_USER_DEFAULT, TIME_NOSECONDS, &sample, NULL,
                                     label, ARRAY_SIZE(label) ))
                GetTextExtentPointW( hdc, label, lstrlenW(label), &clock_size );
            clock_width = max( clock_size.cx + 20, 64 );
        }
        SelectObject( hdc, old_font );
    }
    /* add some margins (FIXME) */
    size.cx += 12 + GetSystemMetrics( SM_CXSMICON );
    size.cy += 4;
    start_button_height = max( icon_cy + 4, size.cy );
    taskbar_button_height = start_button_height;
    clock_height = max( icon_cy + 4, 28 );
    ReleaseDC( 0, hdc );
    DeleteObject( font );

    {
        int screen_width = GetSystemMetrics( SM_CXSCREEN );
        int screen_height = GetSystemMetrics( SM_CYSCREEN );
        BOOL vertical = taskbar_edge == ABE_LEFT || taskbar_edge == ABE_RIGHT;
        int thickness = vertical ? min( max( 144, taskbar_button_width / 2 ), screen_width / 2 ) :
                                   max( icon_cy, size.cy );
        int x = 0, y = 0;

        start_button_width = start_screen_version() == 3 ? 48 : size.cx;
        if (start_screen_version() == 3) start_button_height = 38;
        tray_width = vertical ? thickness : screen_width;
        tray_height = vertical ? screen_height : thickness;
        if (taskbar_edge == ABE_RIGHT) x = screen_width - tray_width;
        if (taskbar_edge == ABE_BOTTOM) y = screen_height - tray_height;
        SetWindowPos( tray_window, 0, x, y, tray_width, tray_height,
                      SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW );
        if (clock_window && taskbar_clock_visible())
            SetWindowPos( clock_window, 0,
                          vertical ? 0 : tray_width - clock_width,
                          vertical ? tray_height - clock_height : 0,
                          vertical ? tray_width : clock_width,
                          vertical ? clock_height : tray_height,
                          SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW );
    }
    layout_systray_icons();
    sync_taskbar_buttons();
    update_taskbar_workarea();
}

static LRESULT WINAPI shell_traywnd_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    switch (msg)
    {
    case WM_COPYDATA:
        return handle_incoming((HWND)wparam, (COPYDATASTRUCT *)lparam);

    case WM_WATER_INPUTLANG_LAYOUT:
        if (enable_taskbar) sync_taskbar_buttons();
        return 0;

    case WM_ERASEBKGND:
        if (enable_taskbar && taskbar_visual != TASKBAR_VISUAL_CLASSIC) return 1;
        break;

    case WM_PAINT:
        if (enable_taskbar && taskbar_visual != TASKBAR_VISUAL_CLASSIC)
        {
            PAINTSTRUCT paint;
            HDC dc = BeginPaint( hwnd, &paint );
            paint_themed_taskbar( hwnd, dc );
            EndPaint( hwnd, &paint );
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
        if (enable_taskbar && (HWND)lparam == clock_window &&
            taskbar_visual != TASKBAR_VISUAL_CLASSIC && taskbar_clock_brush)
        {
            SetTextColor( (HDC)wparam, RGB(255, 255, 255) );
            SetBkColor( (HDC)wparam, taskbar_visual == TASKBAR_VISUAL_LUNA_BLUE ?
                        RGB(50, 137, 211) : RGB(33, 37, 44) );
            return (LRESULT)taskbar_clock_brush;
        }
        break;

    case WM_DISPLAYCHANGE:
        if (!enable_taskbar && (!show_systray || !nb_displayed)) do_hide_systray();
        else do_show_systray();
        break;

    case WM_WINDOWPOSCHANGING:
    {
        WINDOWPOS *p = (WINDOWPOS *)lparam;

        if (p->flags & SWP_SHOWWINDOW && (!show_systray || !nb_displayed) && !enable_taskbar)
        {
            TRACE( "WM_WINDOWPOSCHANGING clearing SWP_SHOWWINDOW.\n" );
            p->flags &= ~SWP_SHOWWINDOW;
        }
        break;
    }

    case WM_MOVE:
        update_systray_balloon_position();
        break;

    case WM_TIMER:
        if (enable_taskbar && wparam == TASKBAR_CLOCK_TIMER)
        {
            update_taskbar_clock();
            return 0;
        }
        break;

    case WM_SYSCOLORCHANGE:
        if (enable_taskbar) InvalidateRect( hwnd, NULL, TRUE );
        break;

    case WM_TIMECHANGE:
        update_taskbar_clock();
        break;

    case WM_SETTINGCHANGE:
        /* SPI_SETWORKAREA is emitted by our own docking code. Avoid
         * rereading settings and recursing into another work-area update. */
        if (enable_taskbar && wparam != SPI_SETWORKAREA)
        {
            taskbar_show_clock = read_taskbar_show_clock();
            quicklaunch_initialize(tray_window, GetSysColor(COLOR_BTNFACE));
            do_show_systray();
        }
        update_taskbar_clock();
        break;

    case WM_LBUTTONDOWN:
        if (enable_taskbar && !taskbar_is_locked() && GetCursorPos( &taskbar_drag_start ))
        {
            taskbar_dragging = TRUE;
            SetCapture( hwnd );
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        if (taskbar_dragging)
        {
            if (wparam & MK_LBUTTON) move_taskbar_from_cursor();
            else
            {
                taskbar_dragging = FALSE;
                if (GetCapture() == hwnd) ReleaseCapture();
            }
            return 0;
        }
        break;

    case WM_LBUTTONUP:
        if (taskbar_dragging)
        {
            move_taskbar_from_cursor();
            taskbar_dragging = FALSE;
            if (GetCapture() == hwnd) ReleaseCapture();
            write_taskbar_settings();
            return 0;
        }
        break;

    case WM_CAPTURECHANGED:
        taskbar_dragging = FALSE;
        break;

    case WM_DESTROY:
        if (taskbar_clock_brush)
        {
            DeleteObject( taskbar_clock_brush );
            taskbar_clock_brush = NULL;
        }
        break;

    case WM_CLOSE:
        /* don't destroy the tray window, just hide it */
        ShowWindow( hwnd, SW_HIDE );
        hide_balloon( balloon_icon );
        show_systray = FALSE;
        return 0;

    case WM_DRAWITEM:
        if (((MEASUREITEMSTRUCT*)lparam)->CtlType == ODT_MENU)
            return menu_wndproc(hwnd, msg, wparam, lparam);
        if (quicklaunch_paint((const DRAWITEMSTRUCT *)lparam)) return TRUE;
        paint_taskbar_button( (const DRAWITEMSTRUCT *)lparam );
        break;

    case WM_MEASUREITEM:
        if (((MEASUREITEMSTRUCT*)lparam)->CtlType == ODT_MENU)
            return menu_wndproc(hwnd, msg, wparam, lparam);
        break;

    case WM_COMMAND:
        if (HIWORD(wparam) == BN_CLICKED)
        {
            if (LOWORD(wparam) == TRAY_MINIMIZE_ALL || LOWORD(wparam) == TRAY_MINIMIZE_ALL_UNDO)
            {
                FIXME( "Shell command %u is not supported.\n", LOWORD(wparam) );
                break;
            }
            if (!quicklaunch_click((HWND)lparam))
                click_taskbar_button( (HWND)lparam );
        }
        break;

    case WM_CONTEXTMENU:
        if (enable_taskbar && taskbar_policy_enabled(L"NoTrayContextMenu")) return 0;
        if (enable_taskbar && (!(HWND)wparam || (HWND)wparam == hwnd || (HWND)wparam == clock_window))
            show_taskbar_menu( hwnd );
        else
            show_taskbar_contextmenu( (HWND)wparam, lparam );
        break;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_INITMENUPOPUP:
    case WM_MENUCOMMAND:
        return menu_wndproc(hwnd, msg, wparam, lparam);

    case WM_USER + 0:
        update_systray_balloon_position();
        return 0;

    case WM_USER + 1:
    {
        struct icon *icon;

        LIST_FOR_EACH_ENTRY( icon, &icon_list, struct icon, entry )
        {
            if (!icon->window) continue;
            hide_icon( icon );
            show_icon( icon );
        }

        return 0;
    }

    default:
        return DefWindowProcW( hwnd, msg, wparam, lparam );
    }
    return 0;
}

/* notification posted to the desktop window */
void handle_parent_notify( HWND hwnd, WPARAM wp )
{
    switch (LOWORD(wp))
    {
    case WM_CREATE:
        add_taskbar_button( hwnd );
        break;
    case WM_DESTROY:
        remove_taskbar_button( hwnd );
        cleanup_systray_window( hwnd );
        break;
    }
    sync_taskbar_buttons();
}

/* On a selected Win9x guest, SysTray.exe—not Explorer—owns the speaker
 * icon and queries winmm for a controllable speaker/headphone mixer.
 * Do not inherit the default Win9x Win16 API profile for an NT desktop. */
static BOOL launch_win9x_systray(void)
{
    struct water_win16_profile profile;
    STARTUPINFOW startup = {.cb = sizeof(startup)};
    PROCESS_INFORMATION process;
    char value[32];
    DWORD len;
    BOOL selected = FALSE;
    WCHAR command[] = L"systray.exe";

    len = GetEnvironmentVariableA(WATER_WIN16_LINE_ENV, value, sizeof(value));
    if (len && len < sizeof(value) && !strcmp(value, WATER_WIN16_LINE_WIN9X))
        selected = TRUE;
    len = GetEnvironmentVariableA(WATER_WIN16_PERSONALITY_ENV, value, sizeof(value));
    if (len && len < sizeof(value) &&
        (!strcmp(value, WATER_WIN16_PERSONALITY_WIN95_OSR2) ||
         !strcmp(value, WATER_WIN16_PERSONALITY_WINME)))
        selected = TRUE;
    if (!selected) return FALSE;

    water_win16_read_profile(&profile);
    if (profile.line != WATER_WIN16_LINE_WIN9X_FAMILY || profile.major != 4)
        return FALSE;

    if (!CreateProcessW(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process))
    {
        WARN("Could not launch Win9x SysTray volume client, error %lu\n", GetLastError());
        return FALSE;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return TRUE;
}

/* this function creates the listener window */

void initialize_systray( BOOL arg_using_root, BOOL arg_enable_shell, BOOL arg_show_systray, BOOL arg_no_tray_items )
{
    shell_traywnd_class.hIcon = LoadIconW( 0, (const WCHAR *)IDI_WINLOGO );
    shell_traywnd_class.hCursor = LoadCursorW( 0, (const WCHAR *)IDC_ARROW );
    tray_icon_class.hIcon = shell_traywnd_class.hIcon;
    tray_icon_class.hCursor = shell_traywnd_class.hCursor;

    icon_cx = GetSystemMetrics( SM_CXSMICON ) + 2*ICON_BORDER;
    icon_cy = GetSystemMetrics( SM_CYSMICON ) + 2*ICON_BORDER;

    if (arg_using_root)
    {
        show_systray = arg_show_systray;
        enable_taskbar = FALSE;
        enable_dock = TRUE;
    }
    else
    {
        show_systray = arg_show_systray && !arg_enable_shell;
        enable_taskbar = arg_enable_shell;
        enable_dock = FALSE;
    }

    no_tray_items = arg_no_tray_items;

    taskbar_visual = enable_taskbar ? get_default_taskbar_visual() : TASKBAR_VISUAL_CLASSIC;
    shell_traywnd_class.hbrBackground =
        taskbar_visual == TASKBAR_VISUAL_CLASSIC ? (HBRUSH)(COLOR_BTNFACE + 1) : NULL;
    if (taskbar_visual != TASKBAR_VISUAL_CLASSIC)
        taskbar_clock_brush = CreateSolidBrush( taskbar_visual == TASKBAR_VISUAL_LUNA_BLUE ?
                                                RGB(50, 137, 211) : RGB(33, 37, 44) );

    /* register the systray listener window class */
    if (!RegisterClassExW( &shell_traywnd_class ))
    {
        ERR( "Could not register SysTray window class\n" );
        return;
    }
    if (!RegisterClassExW( &tray_icon_class ))
    {
        ERR( "Could not register Wine SysTray window classes\n" );
        return;
    }

    if (enable_taskbar)
    {
        read_taskbar_settings();
        /* do_show_systray() applies the saved edge after creating the clock. */
        tray_window = CreateWindowExW( WS_EX_NOACTIVATE, shell_traywnd_class.lpszClassName,
                                       NULL, WS_POPUP | WS_CLIPCHILDREN, 0, 0, 0, 0, 0, 0, 0, 0 );
    }
    else
    {
        SIZE size = get_window_size();
        tray_window = CreateWindowExW( 0, shell_traywnd_class.lpszClassName, L"", WS_CAPTION | WS_SYSMENU,
                                       CW_USEDEFAULT, CW_USEDEFAULT, size.cx, size.cy, 0, 0, 0, 0 );
        NtUserMessageCall( tray_window, WINE_SYSTRAY_DOCK_INIT, 0, 0, NULL, NtUserSystemTrayCall, FALSE );
    }

    if (!tray_window)
    {
        ERR( "Could not create tray window\n" );
        return;
    }

    LoadStringW( NULL, IDS_START_LABEL, start_label, ARRAY_SIZE( start_label ));

    if (enable_taskbar)
        quicklaunch_initialize(tray_window, GetSysColor(COLOR_BTNFACE));

    if (enable_taskbar)
    {
        clock_window = CreateWindowW( L"Static", L"", WS_CHILD | SS_CENTER | SS_CENTERIMAGE,
                                      0, 0, 0, 0, tray_window, NULL, NULL, NULL );
        if (!clock_window) WARN( "Could not create taskbar clock\n" );
        else
        {
            SendMessageW( clock_window, WM_SETFONT,
                          (WPARAM)GetStockObject( DEFAULT_GUI_FONT ), FALSE );
            if (taskbar_clock_visible()) ShowWindow(clock_window, SW_SHOWNOACTIVATE);
        }
        SetTimer( tray_window, TASKBAR_CLOCK_TIMER, 30000, NULL );
        update_taskbar_clock();

        /* Only an installed second layout activates the indicator.
         * Its HWND stays alive while hidden so new layouts are detected. */
        if (!no_tray_items)
        {
            COLORREF background = taskbar_visual == TASKBAR_VISUAL_LUNA_BLUE ?
                     RGB(34, 103, 195) : taskbar_visual == TASKBAR_VISUAL_VISTA_AERO ?
                     RGB(39, 43, 51) : GetSysColor(COLOR_BTNFACE);
            COLORREF foreground = taskbar_visual == TASKBAR_VISUAL_CLASSIC ?
                     GetSysColor(COLOR_BTNTEXT) : RGB(255, 255, 255);
            inputlang_initialize(tray_window, background, foreground);
        }
    }

    /* Windows 8 RTM has a Start hot corner, not a taskbar Start button. */
    if (start_screen_version() != 2) add_taskbar_button( 0 );

    if (enable_taskbar)
    {
        UINT message = RegisterWindowMessageA("TaskbarCreated");

        do_show_systray();
        /* IE 4.0+ shell convention: let already-running applications restore
         * notification icons after Explorer recreates the taskbar. */
        if (message) SendNotifyMessageW(HWND_BROADCAST, message, 0, 0);
        if (!no_tray_items) launch_win9x_systray();
        initialize_start_corner(tray_window);
        /* Windows 8 RTM launches into Start, not directly to the desktop.
         * 8.1 boot-to-desktop policy is a separate configurable feature. */
        if (start_screen_version() == 2) show_start_screen(tray_window);
    }
    else do_hide_systray();
}
