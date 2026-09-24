/*
 * Windows sound volume control
 *
 * Clean-room compatibility implementation of SNDVOL32.EXE.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdlib.h>
#include <string.h>

#include "windows.h"
#include "commctrl.h"
#include "mmsystem.h"

#define IDC_VOLUME 100
#define IDC_MUTE   101
#define IDC_DEVICE 102
#define TIMER_REFRESH 1
#define SLIDER_MAX 1000

struct sndvol_state
{
    HMIXER mixer;
    UINT device_id;
    DWORD line_id;

    BOOL record;
    BOOL have_volume;
    DWORD volume_id;
    DWORD volume_min;
    DWORD volume_max;

    BOOL have_mute;
    DWORD mute_id;

    HWND slider;
    HWND mute;
    HWND device;
    char device_name[MAXPNAMELEN];
};

static BOOL query_control( struct sndvol_state *state, DWORD type, MIXERCONTROLA *control )
{
    MIXERLINECONTROLSA controls;

    memset( control, 0, sizeof(*control) );
    control->cbStruct = sizeof(*control);

    memset( &controls, 0, sizeof(controls) );
    controls.cbStruct = sizeof(controls);
    controls.dwLineID = state->line_id;
    controls.dwControlType = type;
    controls.cControls = 1;
    controls.cbmxctrl = sizeof(*control);
    controls.pamxctrl = control;

    return mixerGetLineControlsA( (HMIXEROBJ)state->mixer, &controls,
                                  MIXER_OBJECTF_HMIXER |
                                  MIXER_GETLINECONTROLSF_ONEBYTYPE ) == MMSYSERR_NOERROR;
}

static BOOL get_volume( struct sndvol_state *state, DWORD *value )
{
    MIXERCONTROLDETAILS_UNSIGNED detail;
    MIXERCONTROLDETAILS details;

    if (!state->have_volume) return FALSE;

    memset( &details, 0, sizeof(details) );
    details.cbStruct = sizeof(details);
    details.dwControlID = state->volume_id;
    details.cChannels = 1;
    details.cbDetails = sizeof(detail);
    details.paDetails = &detail;

    if (mixerGetControlDetailsA( (HMIXEROBJ)state->mixer, &details,
                                 MIXER_OBJECTF_HMIXER |
                                 MIXER_GETCONTROLDETAILSF_VALUE ) != MMSYSERR_NOERROR)
        return FALSE;

    *value = detail.dwValue;
    return TRUE;
}

static BOOL set_volume( struct sndvol_state *state, DWORD value )
{
    MIXERCONTROLDETAILS_UNSIGNED detail;
    MIXERCONTROLDETAILS details;

    if (!state->have_volume) return FALSE;

    detail.dwValue = value;

    memset( &details, 0, sizeof(details) );
    details.cbStruct = sizeof(details);
    details.dwControlID = state->volume_id;
    details.cChannels = 1;
    details.cbDetails = sizeof(detail);
    details.paDetails = &detail;

    return mixerSetControlDetails( (HMIXEROBJ)state->mixer, &details,
                                   MIXER_OBJECTF_HMIXER |
                                   MIXER_SETCONTROLDETAILSF_VALUE ) == MMSYSERR_NOERROR;
}

static BOOL get_mute( struct sndvol_state *state, BOOL *value )
{
    MIXERCONTROLDETAILS_BOOLEAN detail;
    MIXERCONTROLDETAILS details;

    if (!state->have_mute) return FALSE;

    memset( &details, 0, sizeof(details) );
    details.cbStruct = sizeof(details);
    details.dwControlID = state->mute_id;
    details.cChannels = 1;
    details.cbDetails = sizeof(detail);
    details.paDetails = &detail;

    if (mixerGetControlDetailsA( (HMIXEROBJ)state->mixer, &details,
                                 MIXER_OBJECTF_HMIXER |
                                 MIXER_GETCONTROLDETAILSF_VALUE ) != MMSYSERR_NOERROR)
        return FALSE;

    *value = !!detail.fValue;
    return TRUE;
}

static BOOL set_mute( struct sndvol_state *state, BOOL value )
{
    MIXERCONTROLDETAILS_BOOLEAN detail;
    MIXERCONTROLDETAILS details;

    if (!state->have_mute) return FALSE;

    detail.fValue = value;

    memset( &details, 0, sizeof(details) );
    details.cbStruct = sizeof(details);
    details.dwControlID = state->mute_id;
    details.cChannels = 1;
    details.cbDetails = sizeof(detail);
    details.paDetails = &detail;

    return mixerSetControlDetails( (HMIXEROBJ)state->mixer, &details,
                                   MIXER_OBJECTF_HMIXER |
                                   MIXER_SETCONTROLDETAILSF_VALUE ) == MMSYSERR_NOERROR;
}

static void refresh_controls( struct sndvol_state *state )
{
    DWORD value;
    BOOL muted;

    if (state->have_volume && get_volume( state, &value ))
    {
        DWORD range = state->volume_max - state->volume_min;
        DWORD pos = range ? (value - state->volume_min) * SLIDER_MAX / range : 0;
        SendMessageA( state->slider, TBM_SETPOS, TRUE, pos );
    }

    if (state->have_mute && get_mute( state, &muted ))
        SendMessageA( state->mute, BM_SETCHECK, muted ? BST_CHECKED : BST_UNCHECKED, 0 );
}

static BOOL open_mixer( struct sndvol_state *state, UINT device_id )
{
    MIXERLINEA line;
    MIXERCONTROLA control;
    MIXERCAPSA caps;

    if (mixerOpen( &state->mixer, device_id, 0, 0, CALLBACK_NULL ) != MMSYSERR_NOERROR)
        return FALSE;

    state->device_id = device_id;

    memset( &caps, 0, sizeof(caps) );
    if (mixerGetDevCapsA( device_id, &caps, sizeof(caps) ) == MMSYSERR_NOERROR)
        lstrcpynA( state->device_name, caps.szPname, ARRAY_SIZE(state->device_name) );
    else
        wsprintfA( state->device_name, "Mixer %u", device_id );

    memset( &line, 0, sizeof(line) );
    line.cbStruct = sizeof(line);
    line.dwDestination = 0;
    if (mixerGetLineInfoA( (HMIXEROBJ)state->mixer, &line,
                           MIXER_OBJECTF_HMIXER |
                           MIXER_GETLINEINFOF_DESTINATION ) != MMSYSERR_NOERROR)
    {
        mixerClose( state->mixer );
        state->mixer = NULL;
        return FALSE;
    }

    state->line_id = line.dwLineID;

    state->have_volume = query_control( state, MIXERCONTROL_CONTROLTYPE_VOLUME, &control );
    if (state->have_volume)
    {
        state->volume_id = control.dwControlID;
        state->volume_min = control.Bounds.dwMinimum;
        state->volume_max = control.Bounds.dwMaximum;
        if (state->volume_max <= state->volume_min) state->have_volume = FALSE;
    }

    state->have_mute = query_control( state, MIXERCONTROL_CONTROLTYPE_MUTE, &control );
    if (state->have_mute) state->mute_id = control.dwControlID;

    return TRUE;
}

static BOOL find_default_device( BOOL record, UINT *device_id )
{
    UINT i, count = mixerGetNumDevs();

    for (i = 0; i < count; ++i)
    {
        MIXERLINEA line;

        memset( &line, 0, sizeof(line) );
        line.cbStruct = sizeof(line);
        line.dwDestination = 0;

        if (mixerGetLineInfoA( (HMIXEROBJ)(UINT_PTR)i, &line,
                               MIXER_OBJECTF_MIXER |
                               MIXER_GETLINEINFOF_DESTINATION ) != MMSYSERR_NOERROR)
            continue;

        if ((record && line.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_WAVEIN) ||
            (!record && line.dwComponentType == MIXERLINE_COMPONENTTYPE_DST_SPEAKERS))
        {
            *device_id = i;
            return TRUE;
        }
    }

    return FALSE;
}

static void parse_command_line( const char *cmdline, BOOL *record, UINT *device_id,
                                BOOL *have_device )
{
    const char *p = cmdline;

    while (*p)
    {
        char option;
        char *end;
        unsigned long value;

        while (*p == ' ' || *p == '\t') p++;
        if (*p != '-' && *p != '/')
        {
            while (*p && *p != ' ' && *p != '\t') p++;
            continue;
        }

        option = p[1];
        if (option >= 'a' && option <= 'z') option -= 'a' - 'A';
        p += 2;

        if (option == 'R')
        {
            *record = TRUE;
            continue;
        }

        if (option != 'D') continue;

        while (*p == ' ' || *p == '\t' || *p == ':' || *p == '=') p++;
        value = strtoul( p, &end, 0 );
        if (end != p)
        {
            *device_id = value;
            *have_device = TRUE;
            p = end;
        }
    }
}

static LRESULT CALLBACK wndproc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    struct sndvol_state *state = (struct sndvol_state *)GetWindowLongPtrA( hwnd, GWLP_USERDATA );

    switch (msg)
    {
    case WM_CREATE:
    {
        CREATESTRUCTA *create = (CREATESTRUCTA *)lparam;
        HFONT font = GetStockObject( DEFAULT_GUI_FONT );

        state = create->lpCreateParams;
        SetWindowLongPtrA( hwnd, GWLP_USERDATA, (LONG_PTR)state );

        state->device = CreateWindowExA( 0, "STATIC", state->device_name,
                                         WS_CHILD | WS_VISIBLE,
                                         18, 16, 314, 20, hwnd,
                                         (HMENU)(UINT_PTR)IDC_DEVICE, NULL, NULL );
        state->slider = CreateWindowExA( 0, TRACKBAR_CLASSA, "",
                                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
                                         18, 42, 314, 36, hwnd,
                                         (HMENU)(UINT_PTR)IDC_VOLUME, NULL, NULL );
        state->mute = CreateWindowExA( 0, "BUTTON", "Mute",
                                       WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       18, 84, 110, 24, hwnd,
                                       (HMENU)(UINT_PTR)IDC_MUTE, NULL, NULL );

        SendMessageA( state->device, WM_SETFONT, (WPARAM)font, TRUE );
        SendMessageA( state->mute, WM_SETFONT, (WPARAM)font, TRUE );
        SendMessageA( state->slider, TBM_SETRANGE, TRUE, MAKELONG( 0, SLIDER_MAX ) );
        SendMessageA( state->slider, TBM_SETPAGESIZE, 0, 50 );

        EnableWindow( state->slider, state->have_volume );
        EnableWindow( state->mute, state->have_mute );

        refresh_controls( state );
        SetTimer( hwnd, TIMER_REFRESH, 500, NULL );
        return 0;
    }

    case WM_HSCROLL:
        if (state && (HWND)lparam == state->slider && state->have_volume)
        {
            DWORD pos = SendMessageA( state->slider, TBM_GETPOS, 0, 0 );
            DWORD range = state->volume_max - state->volume_min;
            DWORD value = state->volume_min + pos * range / SLIDER_MAX;
            set_volume( state, value );
        }
        return 0;

    case WM_COMMAND:
        if (state && LOWORD(wparam) == IDC_MUTE && HIWORD(wparam) == BN_CLICKED)
        {
            BOOL muted = SendMessageA( state->mute, BM_GETCHECK, 0, 0 ) == BST_CHECKED;
            set_mute( state, muted );
        }
        return 0;

    case WM_TIMER:
        if (state && wparam == TIMER_REFRESH) refresh_controls( state );
        return 0;

    case WM_DESTROY:
        KillTimer( hwnd, TIMER_REFRESH );
        if (state && state->mixer) mixerClose( state->mixer );
        PostQuitMessage( 0 );
        return 0;
    }

    return DefWindowProcA( hwnd, msg, wparam, lparam );
}

int WINAPI WinMain( HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show )
{
    struct sndvol_state state;
    INITCOMMONCONTROLSEX controls;
    WNDCLASSA class;
    MSG msg;
    HWND hwnd;
    UINT device_id = 0;
    BOOL have_device = FALSE;
    const char *title;

    (void)prev;

    memset( &state, 0, sizeof(state) );
    parse_command_line( cmdline, &state.record, &device_id, &have_device );

    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx( &controls );

    if (!have_device && !find_default_device( state.record, &device_id ))
    {
        MessageBoxA( NULL,
                     state.record ? "No recording mixer device is available."
                                  : "No playback mixer device is available.",
                     "Volume Control", MB_OK | MB_ICONERROR );
        return 1;
    }

    if (!open_mixer( &state, device_id ))
    {
        MessageBoxA( NULL, "The selected mixer device could not be opened.",
                     "Volume Control", MB_OK | MB_ICONERROR );
        return 1;
    }

    memset( &class, 0, sizeof(class) );
    class.lpfnWndProc = wndproc;
    class.hInstance = instance;
    class.hCursor = LoadCursorA( NULL, IDC_ARROW );
    class.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    class.lpszClassName = "WaterSndVol32";

    if (!RegisterClassA( &class ))
    {
        mixerClose( state.mixer );
        return 1;
    }

    title = state.record ? "Recording Control" : "Volume Control";
    hwnd = CreateWindowExA( WS_EX_DLGMODALFRAME, class.lpszClassName, title,
                            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                            CW_USEDEFAULT, CW_USEDEFAULT, 370, 155,
                            NULL, NULL, instance, &state );
    if (!hwnd)
    {
        mixerClose( state.mixer );
        return 1;
    }

    ShowWindow( hwnd, show );
    UpdateWindow( hwnd );

    while (GetMessageA( &msg, NULL, 0, 0 ) > 0)
    {
        TranslateMessage( &msg );
        DispatchMessageA( &msg );
    }

    return (int)msg.wParam;
}
