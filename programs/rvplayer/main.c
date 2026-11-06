/*
 * RealPlayer 4.01 compatibility frontend
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program preserves the Windows 98-era RVPLAYER.EXE identity.
 * RealNetworks codecs and other proprietary components are not included.
 */

#define COBJMACROS

#include <windows.h>
#include <dshow.h>
#include <commdlg.h>
#include <shellapi.h>
#include <oleauto.h>
#include <wchar.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(rvplayer);

static HRESULT wait_for_completion(IMediaEvent *event)
{
    HANDLE event_handle;
    HRESULT hr = S_OK;
    BOOL done = FALSE;

    hr = IMediaEvent_GetEventHandle(event, (OAEVENT *)&event_handle);
    if (FAILED(hr))
        return hr;

    while (!done)
    {
        DWORD wait = MsgWaitForMultipleObjects(1, &event_handle, FALSE, INFINITE, QS_ALLINPUT);

        if (wait == WAIT_OBJECT_0)
        {
            LONG event_code;
            LONG_PTR param1, param2;

            while (IMediaEvent_GetEvent(event, &event_code, &param1, &param2, 0) == S_OK)
            {
                HRESULT event_hr = S_OK;
                BOOL terminal = FALSE;

                TRACE("media event %#lx\n", event_code);

                switch (event_code)
                {
                case EC_COMPLETE:
                case EC_USERABORT:
                    terminal = TRUE;
                    break;

                case EC_ERRORABORT:
                    event_hr = (HRESULT)param1;
                    if (SUCCEEDED(event_hr))
                        event_hr = E_FAIL;
                    terminal = TRUE;
                    break;
                }

                IMediaEvent_FreeEventParams(event, event_code, param1, param2);

                if (terminal)
                {
                    hr = event_hr;
                    done = TRUE;
                    break;
                }
            }
        }
        else if (wait == WAIT_OBJECT_0 + 1)
        {
            MSG message;

            while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                {
                    /* Preserve the quit signal for the outer StartCenter loop. */
                    PostQuitMessage((int)message.wParam);
                    hr = E_ABORT;
                    done = TRUE;
                    break;
                }

                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        else
        {
            hr = wait == WAIT_FAILED ? HRESULT_FROM_WIN32(GetLastError()) : E_UNEXPECTED;
            break;
        }
    }

    return hr;
}

static WCHAR *read_ram_target(const WCHAR *filename)
{
    HANDLE file;
    DWORD size, read;
    char *buffer, *start, *end = NULL;
    WCHAR *target;
    int bytes, chars;

    file = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return NULL;

    size = GetFileSize(file, NULL);
    if (size == INVALID_FILE_SIZE || !size || size > 64 * 1024)
    {
        CloseHandle(file);
        return NULL;
    }

    if (!(buffer = HeapAlloc(GetProcessHeap(), 0, size + 1)))
    {
        CloseHandle(file);
        return NULL;
    }

    if (!ReadFile(file, buffer, size, &read, NULL))
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        CloseHandle(file);
        return NULL;
    }
    CloseHandle(file);
    buffer[read] = 0;

    start = buffer;
    for (;;)
    {
        while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n')
            start++;

        if (!*start)
            break;

        if (*start == '#')
        {
            while (*start && *start != '\r' && *start != '\n')
                start++;
            continue;
        }

        end = start;
        while (*end && *end != '\r' && *end != '\n')
            end++;
        while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
            end--;

        if (end > start)
            break;

        start = end;
    }

    if (!*start || end <= start)
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        return NULL;
    }

    bytes = (int)(end - start);
    chars = MultiByteToWideChar(CP_ACP, 0, start, bytes, NULL, 0);
    if (!chars || !(target = HeapAlloc(GetProcessHeap(), 0, (chars + 1) * sizeof(*target))))
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        return NULL;
    }

    MultiByteToWideChar(CP_ACP, 0, start, bytes, target, chars);
    target[chars] = 0;
    HeapFree(GetProcessHeap(), 0, buffer);

    TRACE("resolved RAM metafile %s to %s\n", debugstr_w(filename), debugstr_w(target));
    return target;
}

static WCHAR *resolve_target(const WCHAR *target)
{
    const WCHAR *extension;

    extension = wcsrchr(target, '.');
    if (!extension || lstrcmpiW(extension, L".ram"))
        return NULL;

    if (GetFileAttributesW(target) == INVALID_FILE_ATTRIBUTES)
        return NULL;

    return read_ram_target(target);
}

static HRESULT play_target(const WCHAR *target)
{
    IMediaControl *control = NULL;
    IMediaEvent *event = NULL;
    WCHAR *resolved = NULL;
    const WCHAR *play_target = target;
    BSTR filename;
    HRESULT hr;

    if ((resolved = resolve_target(target)))
        play_target = resolved;

    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMediaControl, (void **)&control);
    if (FAILED(hr))
    {
        ERR("failed to create filter graph, hr %#lx\n", hr);
        goto done;
    }

    if (!(filename = SysAllocString(play_target)))
    {
        hr = E_OUTOFMEMORY;
        goto done;
    }

    hr = IMediaControl_RenderFile(control, filename);
    SysFreeString(filename);
    if (FAILED(hr))
    {
        ERR("failed to render %s, hr %#lx; a compatible RealMedia filter may be required\n",
            debugstr_w(play_target), hr);
        goto done;
    }

    hr = IMediaControl_QueryInterface(control, &IID_IMediaEvent, (void **)&event);
    if (FAILED(hr))
        goto done;

    hr = IMediaControl_Run(control);
    if (SUCCEEDED(hr))
        hr = wait_for_completion(event);

    IMediaControl_Stop(control);

done:
    if (event)
        IMediaEvent_Release(event);
    if (control)
        IMediaControl_Release(control);
    if (resolved)
        HeapFree(GetProcessHeap(), 0, resolved);

    TRACE("playback ended, hr %#lx\n", hr);
    return hr;
}


/*
 * RealPlayer G2 and later added StartCenter notification-area behavior.
 * Keep this explicit compatibility mode separate from the default RealPlayer
 * 4.01 identity: no automatic startup registration or historical 4.x claim.
 * The generic Windows icon is a placeholder until an authentic licensed
 * RealPlayer icon resource is available.
 */
#define STARTCENTER_CALLBACK (WM_APP + 42)
#define STARTCENTER_ICON_ID 1
#define STARTCENTER_OPEN 1001
#define STARTCENTER_CLOSE 1002

static UINT taskbar_created_message;
static BOOL startcenter_icon_added;

static void startcenter_fill_nid(HWND hwnd, NOTIFYICONDATAA *nid)
{
    ZeroMemory(nid, sizeof(*nid));
    /* Use the Windows 95/98-era notification structure, not XP extensions. */
    nid->cbSize = NOTIFYICONDATAA_V1_SIZE;
    nid->hWnd = hwnd;
    nid->uID = STARTCENTER_ICON_ID;
    nid->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid->uCallbackMessage = STARTCENTER_CALLBACK;
    nid->hIcon = LoadIconW(NULL, IDI_APPLICATION);
    lstrcpynA(nid->szTip, "RealPlayer G2 StartCenter (WATER)", 64);
}

static BOOL startcenter_add_icon(HWND hwnd)
{
    NOTIFYICONDATAA nid;

    startcenter_fill_nid(hwnd, &nid);
    startcenter_icon_added = Shell_NotifyIconA(NIM_ADD, &nid);
    if (!startcenter_icon_added)
        WARN("Failed to register RealPlayer StartCenter notification icon\n");
    return startcenter_icon_added;
}

static void startcenter_delete_icon(HWND hwnd)
{
    NOTIFYICONDATAA nid;

    if (!startcenter_icon_added) return;
    startcenter_fill_nid(hwnd, &nid);
    Shell_NotifyIconA(NIM_DELETE, &nid);
    startcenter_icon_added = FALSE;
}

static void startcenter_open_media(HWND hwnd)
{
    OPENFILENAMEA ofn = {0};
    CHAR filename[MAX_PATH] = {0};
    WCHAR target[MAX_PATH];
    HRESULT hr;

    /* The ANSI common dialog is available on actual Windows 98 systems. */
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFilter = "RealMedia files\0*.ra;*.ram;*.rm;*.rv\0All files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = ARRAY_SIZE(filename);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameA(&ofn)) return;
    if (!MultiByteToWideChar(CP_ACP, 0, filename, -1,
                             target, ARRAY_SIZE(target)))
    {
        WARN("Failed to convert selected media filename\n");
        return;
    }

    hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        hr = play_target(target);
        CoUninitialize();
    }
    if (FAILED(hr))
        MessageBoxW(hwnd, L"Playback failed. A compatible RealMedia DirectShow filter "
                    L"may be required.", L"RealPlayer G2 compatibility mode",
                    MB_ICONWARNING | MB_OK);
}

static void startcenter_show_menu(HWND hwnd)
{
    HMENU menu;
    POINT point;
    UINT command;

    if (!(menu = CreatePopupMenu())) return;
    AppendMenuW(menu, MF_STRING, STARTCENTER_OPEN, L"Open media...");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, STARTCENTER_CLOSE, L"Close StartCenter");

    if (GetCursorPos(&point))
    {
        SetForegroundWindow(hwnd);
        command = TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
                                 point.x, point.y, 0, hwnd, NULL);
        /* See the Win9x notification-area popup-dismissal convention. */
        PostMessageW(hwnd, WM_NULL, 0, 0);
        if (command == STARTCENTER_OPEN) startcenter_open_media(hwnd);
        if (command == STARTCENTER_CLOSE) DestroyWindow(hwnd);
    }
    DestroyMenu(menu);
}

static LRESULT CALLBACK startcenter_wndproc(HWND hwnd, UINT message,
                                            WPARAM wparam, LPARAM lparam)
{
    if (taskbar_created_message && message == taskbar_created_message)
    {
        /* Explorer was restarted: the new notification area has no old icon. */
        startcenter_icon_added = FALSE;
        startcenter_add_icon(hwnd);
        return 0;
    }

    if (message == STARTCENTER_CALLBACK && wparam == STARTCENTER_ICON_ID)
    {
        switch (lparam)
        {
        case WM_LBUTTONDBLCLK:
            startcenter_open_media(hwnd);
            return 0;
        case WM_RBUTTONUP:
            startcenter_show_menu(hwnd);
            return 0;
        }
    }

    switch (message)
    {
    case WM_DESTROY:
        startcenter_delete_icon(hwnd);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

/* WATER-specific opt-in for the later G2 StartCenter behavior. */
static int run_g2_startcenter(void)
{
    static const WCHAR class_name[] = L"WaterRealPlayerG2StartCenter";
    WNDCLASSW wc = {0};
    HINSTANCE instance = GetModuleHandleW(NULL);
    HWND hwnd;
    MSG msg;
    BOOL status;

    wc.lpfnWndProc = startcenter_wndproc;
    wc.hInstance = instance;
    wc.lpszClassName = class_name;
    if (!RegisterClassW(&wc))
        return 1;

    taskbar_created_message = RegisterWindowMessageA("TaskbarCreated");
    hwnd = CreateWindowW(class_name, L"RealPlayer G2 StartCenter", WS_OVERLAPPED,
                         0, 0, 0, 0, NULL, NULL, instance, NULL);
    if (!hwnd) return 1;

    if (!startcenter_add_icon(hwnd))
    {
        DestroyWindow(hwnd);
        return 1;
    }

    while ((status = GetMessageW(&msg, NULL, 0, 0)) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (IsWindow(hwnd)) DestroyWindow(hwnd);
    return status == -1 ? 1 : 0;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    const WCHAR *target;
    HRESULT hr;

    if (argc == 2 && !wcscmp(argv[1], L"--compat-g2-startcenter"))
        return run_g2_startcenter();

    if (argc < 2)
    {
        FIXME("RealPlayer 4.01 UI without a target is not implemented yet\n");
        return 0;
    }

    /* RealPlayer launchers may add switches before the media target. */
    target = argv[argc - 1];

    hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        ERR("CoInitialize failed, hr %#lx\n", hr);
        return 1;
    }

    hr = play_target(target);
    CoUninitialize();

    return FAILED(hr);
}
