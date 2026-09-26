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

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(nsplayer);

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
                    terminal = TRUE;
                    break;

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

    IMediaControl_Stop(control);
    IMediaEvent_Release(event);
    IMediaControl_Release(control);

    TRACE("playback ended, hr %#lx\n", hr);
    return hr;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    HRESULT hr;

    if (argc < 2)
    {
        FIXME("NetShow Player UI without a target is not implemented yet\n");
        return 0;
    }

    hr = CoInitialize(NULL);
    if (FAILED(hr))
    {
        ERR("CoInitialize failed, hr %#lx\n", hr);
        return 1;
    }

    hr = play_target(argv[1]);
    CoUninitialize();

    return FAILED(hr);
}
