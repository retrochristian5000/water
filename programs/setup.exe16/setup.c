/*
 * Windows 3.x Setup
 *
 * This program implements the installed Windows Setup system-settings editor.
 * It intentionally starts with the SYSTEM.INI [boot] driver assignments that
 * Windows 3.1 Setup used for hardware configuration.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "wine/winbase16.h"
#include "wine/winuser16.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(setup);

#define ID_FIRST_DRIVER  100
#define ID_APPLY         200
#define ID_CLOSE         201

struct driver_setting
{
    const char *label;
    const char *key;
    HWND16 edit;
    char value[128];
};

static HINSTANCE16 setup_instance;
static const char setup_class[] = "WaterSetup16";

static struct driver_setting driver_settings[] =
{
    { "System:",   "system.drv" },
    { "Display:",  "display.drv" },
    { "Keyboard:", "keyboard.drv" },
    { "Mouse:",    "mouse.drv" },
    { "Sound:",    "sound.drv" },
    { "Comm:",     "comm.drv" },
    { "Network:",  "network.drv" },
};

static void load_driver_setting( struct driver_setting *setting )
{
    GetPrivateProfileString16( "boot", setting->key, "", setting->value,
                               sizeof(setting->value), "system.ini" );
}

static void create_controls( HWND16 hwnd )
{
    unsigned int i;
    INT16 y = 18;

    CreateWindow16( "STATIC", "Change System Settings", WS_CHILD | WS_VISIBLE,
                    12, 8, 180, 18, hwnd, 0, setup_instance, 0 );

    y = 34;
    for (i = 0; i < sizeof(driver_settings) / sizeof(driver_settings[0]); i++, y += 27)
    {
        struct driver_setting *setting = &driver_settings[i];

        load_driver_setting( setting );
        CreateWindow16( "STATIC", setting->label, WS_CHILD | WS_VISIBLE,
                        12, y + 3, 88, 18, hwnd, 0, setup_instance, 0 );
        setting->edit = CreateWindow16( "EDIT", setting->value,
                        WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                        104, y, 318, 21, hwnd, (HMENU16)(ID_FIRST_DRIVER + i),
                        setup_instance, 0 );
    }

    CreateWindow16( "STATIC",
                    "Changes are written to SYSTEM.INI and take effect after Windows restarts.",
                    WS_CHILD | WS_VISIBLE, 12, y + 2, 410, 18, hwnd, 0, setup_instance, 0 );

    CreateWindow16( "BUTTON", "Apply",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
                    266, y + 28, 74, 24, hwnd, (HMENU16)ID_APPLY, setup_instance, 0 );
    CreateWindow16( "BUTTON", "Close",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    348, y + 28, 74, 24, hwnd, (HMENU16)ID_CLOSE, setup_instance, 0 );
}

static BOOL16 save_driver_settings( HWND16 hwnd )
{
    unsigned int i;
    BOOL16 changed = FALSE;

    for (i = 0; i < sizeof(driver_settings) / sizeof(driver_settings[0]); i++)
    {
        struct driver_setting *setting = &driver_settings[i];
        char value[sizeof(setting->value)];
        SEGPTR segptr = MapLS( value );

        value[0] = 0;
        GetWindowText16( setting->edit, segptr, sizeof(value) );
        UnMapLS( segptr );

        if (!strcmp( value, setting->value )) continue;

        if (!WritePrivateProfileString16( "boot", setting->key,
                                          value[0] ? value : NULL, "system.ini" ))
        {
            char message[192];

            snprintf( message, sizeof(message), "Could not update [boot] %s in SYSTEM.INI.",
                      setting->key );
            MessageBox16( hwnd, message, "Windows Setup", MB_OK | MB_ICONERROR );
            return FALSE;
        }

        strcpy( setting->value, value );
        changed = TRUE;
        TRACE( "updated [boot] %s=%s\n", setting->key, wine_dbgstr_a(value) );
    }

    if (changed)
        MessageBox16( hwnd,
                      "System settings were updated. Restart Windows before relying on the new drivers.",
                      "Windows Setup", MB_OK | MB_ICONINFORMATION );
    else
        MessageBox16( hwnd, "No system settings were changed.",
                      "Windows Setup", MB_OK | MB_ICONINFORMATION );

    return TRUE;
}

static LRESULT CALLBACK setup_wndproc( HWND16 hwnd, UINT16 msg, WPARAM16 wparam, LPARAM lparam )
{
    switch (msg)
    {
    case WM_COMMAND:
        switch (wparam)
        {
        case ID_APPLY:
            save_driver_settings( hwnd );
            return 0;
        case ID_CLOSE:
            DestroyWindow16( hwnd );
            return 0;
        }
        break;

    case WM_CLOSE:
        DestroyWindow16( hwnd );
        return 0;

    case WM_DESTROY:
        PostQuitMessage16( 0 );
        return 0;
    }

    return DefWindowProc16( hwnd, msg, wparam, lparam );
}

WORD WINAPI WinMain16( HINSTANCE16 instance, HINSTANCE16 prev, LPSTR cmdline, WORD show )
{
    WNDCLASS16 class;
    MSG16 msg;
    HWND16 hwnd;
    SEGPTR class_name;

    memset( &class, 0, sizeof(class) );
    setup_instance = instance;

    if (!prev)
    {
        class.style = CS_HREDRAW | CS_VREDRAW;
        class.lpfnWndProc = setup_wndproc;
        class.hInstance = instance;
        class.hbrBackground = (HBRUSH16)(COLOR_BTNFACE + 1);
        class_name = MapLS( (void *)setup_class );
        class.lpszClassName = class_name;
        if (!RegisterClass16( &class ))
        {
            UnMapLS( class_name );
            MessageBox16( 0, "Could not register the Windows Setup window class.",
                          "Windows Setup", MB_OK | MB_ICONERROR );
            return 1;
        }
        UnMapLS( class_name );
    }

    hwnd = CreateWindow16( setup_class, "Windows Setup",
                           WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                           CW_USEDEFAULT16, CW_USEDEFAULT16, 450, 300,
                           0, 0, instance, 0 );
    if (!hwnd)
    {
        MessageBox16( 0, "Could not create the Windows Setup window.",
                      "Windows Setup", MB_OK | MB_ICONERROR );
        return 1;
    }

    create_controls( hwnd );
    ShowWindow16( hwnd, show );
    UpdateWindow16( hwnd );

    while (GetMessage16( &msg, 0, 0, 0 ))
    {
        TranslateMessage16( &msg );
        DispatchMessage16( &msg );
    }

    return msg.wParam;
}
