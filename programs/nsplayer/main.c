/*
 * Microsoft NetShow Player compatibility frontend
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program preserves the historical NSPLAYER.EXE identity separately
 * from WMPLAYER.EXE.  NetShow Player 2.0 was used for ASF/ASX streaming and
 * was installed under Program Files\\Microsoft NetShow\\Player.
 */

#define COBJMACROS

#include <windows.h>
#include <dshow.h>
#include <oleauto.h>
#include <commdlg.h>
#include "nsplayer.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(nsplayer);

static HRESULT wait_for_completion(IMediaEvent *event)
{
    OAEVENT graph_event;
    HANDLE event_handle;
    HRESULT hr = S_OK;
    BOOL done = FALSE;

    hr = IMediaEvent_GetEventHandle(event, &graph_event);
    if (FAILED(hr))
        return hr;
    event_handle = (HANDLE)graph_event;
    if (!event_handle) return E_UNEXPECTED;

    while (!done)
    {
        DWORD wait = MsgWaitForMultipleObjectsEx(1, &event_handle, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);

        if (wait == WAIT_OBJECT_0)
        {
            LONG event_code;
            LONG_PTR param1, param2;

            while (IMediaEvent_GetEvent(event, &event_code, &param1, &param2, 0) == S_OK)
            {
                HRESULT event_hr = S_OK;
                BOOL terminal = FALSE;

                TRACE("media event %#lx\n", event_code);

                terminal = nsplayer_terminal_event(event_code, param1, &event_hr);

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

static HRESULT play_target(const WCHAR *target)
{
    IMediaControl *control = NULL;
    IMediaEvent *event = NULL;
    BSTR filename;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IMediaControl, (void **)&control);
    if (FAILED(hr))
    {
        ERR("failed to create filter graph, hr %#lx\n", hr);
        return hr;
    }

    if (!(filename = SysAllocString(target)))
    {
        IMediaControl_Release(control);
        return E_OUTOFMEMORY;
    }

    hr = IMediaControl_RenderFile(control, filename);
    SysFreeString(filename);
    if (FAILED(hr))
    {
        ERR("failed to render %s, hr %#lx\n", debugstr_w(target), hr);
        IMediaControl_Release(control);
        return hr;
    }

    hr = IMediaControl_QueryInterface(control, &IID_IMediaEvent, (void **)&event);
    if (FAILED(hr))
    {
        IMediaControl_Release(control);
        return hr;
    }

    hr = IMediaControl_Run(control);
    if (SUCCEEDED(hr))
        hr = wait_for_completion(event);

    {
        HRESULT stop_hr = IMediaControl_Stop(control);
        if (SUCCEEDED(hr) && FAILED(stop_hr))
            hr = stop_hr;
    }
    IMediaEvent_Release(event);
    IMediaControl_Release(control);

    TRACE("playback ended, hr %#lx\n", hr);
    return hr;
}

static void show_error(HRESULT hr)
{
    WCHAR message[128];

    swprintf(message, ARRAY_SIZE(message), L"NetShow could not play the selected media (error 0x%08lx).",
            (unsigned long)hr);
    MessageBoxW(NULL, message, L"NetShow Player", MB_OK | MB_ICONERROR);
}

static HRESULT choose_file(WCHAR *path, DWORD capacity)
{
    static const WCHAR filters[] = L"NetShow media (*.asf;*.asx;*.nsc)\0*.asf;*.asx;*.nsc\0"
            L"All files (*.*)\0*.*\0";
    OPENFILENAMEW dialog = {0};
    DWORD error;

    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFilter = filters;
    dialog.lpstrFile = path;
    dialog.nMaxFile = capacity;
    dialog.lpstrTitle = L"NetShow Player - Open File";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&dialog))
        return S_OK;
    if (!(error = CommDlgExtendedError()))
        return S_FALSE;
    WARN("Open file dialog failed: %#lx\n", error);
    return E_FAIL;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    WCHAR path[32768] = {0};
    const WCHAR *target;
    enum nsplayer_action action;
    HRESULT hr;

    action = nsplayer_get_action(argc, (const WCHAR *const *)argv);
    if (action == NSPLAYER_HELP || action == NSPLAYER_INVALID)
    {
        MessageBoxW(NULL, L"Usage: nsplayer.exe [media file or location]", L"NetShow Player",
                MB_OK | (action == NSPLAYER_HELP ? MB_ICONINFORMATION : MB_ICONERROR));
        return action == NSPLAYER_INVALID;
    }

    hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        show_error(hr);
        return 1;
    }

    target = argc > 1 ? argv[1] : path;
    if (action == NSPLAYER_OPEN_DIALOG)
    {
        hr = choose_file(path, ARRAY_SIZE(path));
        if (hr != S_OK)
        {
            if (FAILED(hr)) show_error(hr);
            CoUninitialize();
            return FAILED(hr);
        }
    }

    hr = play_target(target);
    if (FAILED(hr) && hr != E_ABORT)
        show_error(hr);
    CoUninitialize();

    return FAILED(hr);
}
