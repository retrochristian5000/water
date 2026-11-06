/*
 * Explorer private definitions
 *
 * Copyright 2006 Alexandre Julliard
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

#ifndef __WINE_EXPLORER_PRIVATE_H
#define __WINE_EXPLORER_PRIVATE_H

extern void manage_desktop( WCHAR *arg );
extern void initialize_systray( BOOL using_root, BOOL enable_shell, BOOL show_systray, BOOL no_tray_items );
extern void initialize_appbar(void);
extern BOOL get_taskbar_info( RECT *rect, UINT *state, UINT *edge );
extern void notify_appbar_taskbar_moved(void);

/* A private layout request; only WATER's input-language child posts this. */
#define WM_WATER_INPUTLANG_LAYOUT (WM_APP + 0x127)
extern void inputlang_initialize(HWND owner, COLORREF background, COLORREF text);

/* IE4-era Quick Launch versus Windows 7+ unified pinned buttons. */
extern UINT quicklaunch_mode(void);
extern BOOL quicklaunch_can_toggle(void);
extern BOOL quicklaunch_is_enabled(void);
extern void quicklaunch_set_enabled(BOOL enabled);
extern void quicklaunch_initialize(HWND owner, COLORREF background);
extern void quicklaunch_reset_running(void);
extern BOOL quicklaunch_bind_window(HWND hwnd, const WCHAR *executable, BOOL active);
extern int quicklaunch_layout(int pos, int right, BOOL vertical,
                              int full_width, int full_height);
extern BOOL quicklaunch_paint(const DRAWITEMSTRUCT *draw);
extern BOOL quicklaunch_click(HWND button);
extern int inputlang_extent(BOOL vertical);
extern void inputlang_layout(int width, int height, int clock_width, int clock_height,
                             int icon_width, int icon_height, UINT icon_count, UINT edge);
extern void handle_parent_notify( HWND hwnd, WPARAM wp );
extern void do_startmenu( HWND owner );
extern BOOL shell_account_classic_header(void);
extern void shell_account_display_name( WCHAR *name, UINT count );
extern void paint_shell_account( HDC dc, const RECT *rect, BOOL start_screen );
extern void edit_shell_account( HWND owner );
extern UINT start_screen_version(void);
extern void show_start_screen(HWND owner);
extern void initialize_start_corner(HWND owner);
extern LRESULT menu_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

#endif  /* __WINE_EXPLORER_PRIVATE_H */
