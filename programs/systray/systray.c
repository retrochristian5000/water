/*
 * Win9x SysTray volume notification client.
 *
 * The shell owns the notification area; SysTray owns the speaker icon.
 * SndVol32 owns the full mixer UI. The Win9x volume icon is shown only
 * when a controllable playback mixer and the user's SysTray setting exist.
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
#include "shellapi.h"

#define TRAY_ICON_ID       1
#define WM_TRAY_VOLUME     (WM_APP + 21)
#define TIMER_CHECK       1
#define CMD_OPEN_VOLUME   201
#define CMD_MUTE          202
#define IDC_VOLUME_SLIDER 301
#define IDC_MUTE_CHECK    302
#define SLIDER_MAX        1000

struct volume_state
{
    HWND hwnd, popup, slider, mute_box;
    HMIXER mixer;
    HICON icon;
    UINT taskbar_created;
    DWORD volume_id, mute_id, min_volume, max_volume;
    UINT channels, mute_channels;
    BOOL have_mute, icon_added;
};

static struct volume_state volume;

static BOOL tray_enabled(void)
{
    static const char path[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\SysTray";
    HKEY key;
    DWORD services, type = 0, size = sizeof(services);
    LONG ret;

    /* Windows 95/98 default to showing volume when supported. Bit 2
     * of Services controls the optional speaker icon (3=off, 7=on). */
    if (RegOpenKeyExA(HKEY_CURRENT_USER, path, 0, KEY_QUERY_VALUE, &key))
        return TRUE;
    ret = RegQueryValueExA(key, "Services", NULL, &type, (BYTE *)&services, &size);
    RegCloseKey(key);
    if (ret || type != REG_DWORD || size != sizeof(services))
        return TRUE;
    return !!(services & 0x04);
}

static BOOL query_control(HMIXER mixer, DWORD line_id, DWORD type,
                          MIXERCONTROLA *control)
{
    MIXERLINECONTROLSA request;

    memset(control, 0, sizeof(*control));
    control->cbStruct = sizeof(*control);
    memset(&request, 0, sizeof(request));
    request.cbStruct = sizeof(request);
    request.dwLineID = line_id;
    request.dwControlType = type;
    request.cControls = 1;
    request.cbmxctrl = sizeof(*control);
    request.pamxctrl = control;
    return mixerGetLineControlsA((HMIXEROBJ)mixer, &request,
                 MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE) == MMSYSERR_NOERROR;
}

static BOOL get_control_value(DWORD id, BOOL is_boolean, DWORD *value)
{
    MIXERCONTROLDETAILS details;
    MIXERCONTROLDETAILS_UNSIGNED *items;
    BOOL success = FALSE;
    UINT count = is_boolean ? volume.mute_channels : volume.channels, i;
    ULONGLONG total = 0;

    if (!volume.mixer || !count || count > 32) return FALSE;
    if (!(items = calloc(count, sizeof(*items)))) return FALSE;

    memset(&details, 0, sizeof(details));
    details.cbStruct = sizeof(details);
    details.dwControlID = id;
    details.cChannels = count;
    details.cbDetails = sizeof(*items);
    details.paDetails = items;

    if (mixerGetControlDetailsA((HMIXEROBJ)volume.mixer, &details,
                    MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR)
    {
        for (i = 0; i < count; i++) total += items[i].dwValue;
        *value = is_boolean ? !!total : total / count;
        success = TRUE;
    }
    free(items);
    return success;
}

static BOOL set_control_value(DWORD id, BOOL is_boolean, DWORD value)
{
    MIXERCONTROLDETAILS details;
    MIXERCONTROLDETAILS_UNSIGNED *items;
    UINT count = is_boolean ? volume.mute_channels : volume.channels, i;
    BOOL success;

    if (!volume.mixer || !count || count > 32) return FALSE;
    if (!(items = calloc(count, sizeof(*items)))) return FALSE;
    for (i = 0; i < count; i++) items[i].dwValue = value;

    memset(&details, 0, sizeof(details));
    details.cbStruct = sizeof(details);
    details.dwControlID = id;
    details.cChannels = count;
    details.cbDetails = sizeof(*items);
    details.paDetails = items;
    success = mixerSetControlDetails((HMIXEROBJ)volume.mixer, &details,
              MIXER_OBJECTF_HMIXER | MIXER_SETCONTROLDETAILSF_VALUE) == MMSYSERR_NOERROR;
    free(items);
    return success;
}

static void close_mixer(void)
{
    if (volume.mixer) mixerClose(volume.mixer);
    volume.mixer = NULL;
    volume.channels = 0;
    volume.have_mute = FALSE;
    volume.mute_channels = 0;
}

static BOOL open_playback_mixer(void)
{
    UINT device, count = mixerGetNumDevs();

    for (device = 0; device < count; device++)
    {
        MIXERCAPSA caps;
        HMIXER mixer;
        UINT dest;

        if (mixerGetDevCapsA(device, &caps, sizeof(caps)) != MMSYSERR_NOERROR ||
            mixerOpen(&mixer, device, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
            continue;

        for (dest = 0; dest < caps.cDestinations; dest++)
        {
            MIXERLINEA line;
            MIXERCONTROLA control, mute;

            memset(&line, 0, sizeof(line));
            line.cbStruct = sizeof(line);
            line.dwDestination = dest;
            if (mixerGetLineInfoA((HMIXEROBJ)mixer, &line,
                    MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_DESTINATION) != MMSYSERR_NOERROR)
                continue;
            if (line.dwComponentType != MIXERLINE_COMPONENTTYPE_DST_SPEAKERS &&
                line.dwComponentType != MIXERLINE_COMPONENTTYPE_DST_HEADPHONES)
                continue;
            /* An installed wave driver is insufficient: require a usable
             * software volume control on its playback destination. */
            if (!query_control(mixer, line.dwLineID, MIXERCONTROL_CONTROLTYPE_VOLUME, &control) ||
                control.Bounds.dwMaximum <= control.Bounds.dwMinimum)
                continue;

            volume.mixer = mixer;
            volume.volume_id = control.dwControlID;
            volume.min_volume = control.Bounds.dwMinimum;
            volume.max_volume = control.Bounds.dwMaximum;
            volume.channels = (control.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM)
                            ? 1 : max(1, line.cChannels);
            if (volume.channels > 32)
            {
                close_mixer();
                return FALSE;
            }
            volume.have_mute = query_control(mixer, line.dwLineID,
                                             MIXERCONTROL_CONTROLTYPE_MUTE, &mute);
            if (volume.have_mute)
            {
                volume.mute_id = mute.dwControlID;
                volume.mute_channels = (mute.fdwControl & MIXERCONTROL_CONTROLF_UNIFORM)
                                     ? 1 : max(1, line.cChannels);
                if (volume.mute_channels > 32) volume.have_mute = FALSE;
            }
            return TRUE;
        }
        mixerClose(mixer);
    }
    return FALSE;
}

/* A compact original monochrome speaker glyph, not an unlicensed OS icon. */
static HICON create_speaker_icon(void)
{
    static const WORD pixels[16] =
    {
        0x0000, 0x0000, 0x0180, 0x0380,
        0x0788, 0x1f8c, 0x1f86, 0x1f82,
        0x1f82, 0x1f86, 0x1f8c, 0x0788,
        0x0380, 0x0180, 0x0000, 0x0000
    };
    BYTE and_mask[32], xor_mask[32] = {0};
    UINT i;

    for (i = 0; i < ARRAY_SIZE(pixels); i++)
    {
        and_mask[2 * i] = (BYTE)(~pixels[i] >> 8);
        and_mask[2 * i + 1] = (BYTE)~pixels[i];
    }
    return CreateIcon(GetModuleHandleA(NULL), 16, 16, 1, 1, and_mask, xor_mask);
}

static void fill_notify_icon(NOTIFYICONDATAA *data)
{
    memset(data, 0, sizeof(*data));
    data->cbSize = NOTIFYICONDATAA_V1_SIZE; /* Windows 95/98 ABI */
    data->hWnd = volume.hwnd;
    data->uID = TRAY_ICON_ID;
    data->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    data->uCallbackMessage = WM_TRAY_VOLUME;
    data->hIcon = volume.icon;
    lstrcpynA(data->szTip, "Volume", 64);
}

static void remove_icon(void)
{
    NOTIFYICONDATAA nid;

    if (!volume.icon_added) return;
    fill_notify_icon(&nid);
    Shell_NotifyIconA(NIM_DELETE, &nid);
    volume.icon_added = FALSE;
}

static void add_icon(void)
{
    NOTIFYICONDATAA nid;

    if (volume.icon_added || !volume.mixer || !tray_enabled()) return;
    fill_notify_icon(&nid);
    volume.icon_added = Shell_NotifyIconA(NIM_ADD, &nid);
}

static void refresh_popup(void)
{
    DWORD value;

    if (!volume.popup || !IsWindowVisible(volume.popup)) return;
    if (get_control_value(volume.volume_id, FALSE, &value))
    {
        DWORD range = volume.max_volume - volume.min_volume;
        DWORD pos = range ? (value - volume.min_volume) * SLIDER_MAX / range : 0;
        SendMessageA(volume.slider, TBM_SETPOS, TRUE, pos);
    }
    if (volume.have_mute && get_control_value(volume.mute_id, TRUE, &value))
        SendMessageA(volume.mute_box, BM_SETCHECK, !!value, 0);
}

static LRESULT CALLBACK popup_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        volume.slider = CreateWindowExA(0, TRACKBAR_CLASSA, "",
                         WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
                         10, 11, 144, 34, hwnd, (HMENU)(UINT_PTR)IDC_VOLUME_SLIDER, NULL, NULL);
        volume.mute_box = CreateWindowExA(0, "BUTTON", "Mute",
                         WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                         18, 53, 120, 23, hwnd, (HMENU)(UINT_PTR)IDC_MUTE_CHECK, NULL, NULL);
        SendMessageA(volume.slider, TBM_SETRANGE, TRUE, MAKELONG(0, SLIDER_MAX));
        EnableWindow(volume.mute_box, volume.have_mute);
        return 0;

    case WM_HSCROLL:
        if ((HWND)lp == volume.slider && volume.mixer)
        {
            DWORD pos = SendMessageA(volume.slider, TBM_GETPOS, 0, 0);
            DWORD range = volume.max_volume - volume.min_volume;
            set_control_value(volume.volume_id, FALSE,
                              volume.min_volume + pos * range / SLIDER_MAX);
        }
        return 0;

    case WM_COMMAND:
        if (LOWORD(wp) == IDC_MUTE_CHECK && HIWORD(wp) == BN_CLICKED && volume.have_mute)
        {
            BOOL checked = SendMessageA(volume.mute_box, BM_GETCHECK, 0, 0) == BST_CHECKED;
            set_control_value(volume.mute_id, TRUE, checked);
        }
        return 0;

    case WM_ACTIVATE:
        if (LOWORD(wp) == WA_INACTIVE) ShowWindow(hwnd, SW_HIDE);
        return 0;

    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static void show_popup(void)
{
    POINT point;
    int x, y;

    if (!volume.mixer) return;
    if (!volume.popup)
        volume.popup = CreateWindowExA(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                       "WaterSysTrayVolumePopup", "Volume",
                                       WS_POPUP | WS_BORDER, 0, 0, 168, 90,
                                       NULL, NULL, GetModuleHandleA(NULL), NULL);
    if (!volume.popup || !GetCursorPos(&point)) return;
    if (IsWindowVisible(volume.popup))
    {
        ShowWindow(volume.popup, SW_HIDE);
        return;
    }
    x = max(0, min(point.x - 70, GetSystemMetrics(SM_CXSCREEN) - 168));
    y = point.y - 100;
    if (y < 0) y = point.y + 20;
    y = max(0, min(y, GetSystemMetrics(SM_CYSCREEN) - 90));
    SetWindowPos(volume.popup, HWND_TOPMOST, x, y, 168, 90, SWP_SHOWWINDOW);
    refresh_popup();
    SetForegroundWindow(volume.popup);
}

static void open_full_volume(void)
{
    STARTUPINFOA startup = {0};
    PROCESS_INFORMATION proc;
    char command[] = "sndvol32.exe";

    startup.cb = sizeof(startup);
    if (CreateProcessA(NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &proc))
    {
        CloseHandle(proc.hThread);
        CloseHandle(proc.hProcess);
    }
}

static void show_context_menu(void)
{
    HMENU menu;
    POINT point;
    UINT action;
    DWORD mute = 0;

    if (!(menu = CreatePopupMenu())) return;
    AppendMenuA(menu, MF_STRING, CMD_OPEN_VOLUME, "Open Volume Controls");
    if (volume.have_mute)
    {
        get_control_value(volume.mute_id, TRUE, &mute);
        AppendMenuA(menu, MF_STRING | (mute ? MF_CHECKED : 0), CMD_MUTE, "Mute");
    }
    if (GetCursorPos(&point))
    {
        SetForegroundWindow(volume.hwnd);
        action = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                point.x, point.y, 0, volume.hwnd, NULL);
        PostMessageA(volume.hwnd, WM_NULL, 0, 0);
        if (action == CMD_OPEN_VOLUME) open_full_volume();
        else if (action == CMD_MUTE && volume.have_mute)
            set_control_value(volume.mute_id, TRUE, !mute);
    }
    DestroyMenu(menu);
}

static void check_audio(void)
{
    DWORD current;

    if (!tray_enabled())
    {
        remove_icon();
        if (volume.popup) ShowWindow(volume.popup, SW_HIDE);
        return;
    }

    if (volume.mixer && !get_control_value(volume.volume_id, FALSE, &current))
    {
        remove_icon();
        close_mixer();
    }
    if (!volume.mixer) open_playback_mixer();
    if (volume.mixer) add_icon();
    refresh_popup();
}

static LRESULT CALLBACK tray_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (volume.taskbar_created && msg == volume.taskbar_created)
    {
        volume.icon_added = FALSE; /* Explorer discarded its former tray. */
        check_audio();
        return 0;
    }

    if (msg == WM_TRAY_VOLUME && wp == TRAY_ICON_ID)
    {
        switch (lp)
        {
        case WM_LBUTTONUP:    show_popup(); return 0;
        case WM_LBUTTONDBLCLK:
            if (volume.popup) ShowWindow(volume.popup, SW_HIDE);
            open_full_volume();
            return 0;
        case WM_RBUTTONUP:    show_context_menu(); return 0;
        }
    }

    switch (msg)
    {
    case WM_TIMER:
        if (wp == TIMER_CHECK) check_audio();
        return 0;
    case WM_DEVICECHANGE:
        remove_icon();
        close_mixer();
        check_audio();
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_CHECK);
        remove_icon();
        if (volume.popup) DestroyWindow(volume.popup);
        close_mixer();
        if (volume.icon) DestroyIcon(volume.icon);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR command, int show)
{
    static const char class_name[] = "WaterWin9xSysTrayVolume";
    static const char popup_name[] = "WaterSysTrayVolumePopup";
    INITCOMMONCONTROLSEX controls;
    WNDCLASSA wc;
    MSG msg;
    BOOL result;

    (void)previous;
    (void)command;
    (void)show;
    if (FindWindowA(class_name, NULL)) return 0; /* one per desktop */

    memset(&controls, 0, sizeof(controls));
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_BAR_CLASSES;
    InitCommonControlsEx(&controls);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = popup_wndproc;
    wc.hInstance = instance;
    wc.lpszClassName = popup_name;
    if (!RegisterClassA(&wc)) return 1;

    wc.lpfnWndProc = tray_wndproc;
    wc.lpszClassName = class_name;
    if (!RegisterClassA(&wc)) return 1;

    volume.hwnd = CreateWindowExA(0, class_name, "", WS_OVERLAPPED,
                                  0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (!volume.hwnd) return 1;

    volume.icon = create_speaker_icon();
    volume.taskbar_created = RegisterWindowMessageA("TaskbarCreated");
    check_audio();
    SetTimer(volume.hwnd, TIMER_CHECK, 5000, NULL);

    while ((result = GetMessageA(&msg, NULL, 0, 0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (IsWindow(volume.hwnd)) DestroyWindow(volume.hwnd);
    return result == -1 ? 1 : (int)msg.wParam;
}
