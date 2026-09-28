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
    char *buffer, *start, *end;
    WCHAR *target;
    int chars;

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

    chars = MultiByteToWideChar(CP_ACP, 0, start, end - start, NULL, 0);
    if (!chars || !(target = HeapAlloc(GetProcessHeap(), 0, (chars + 1) * sizeof(*target))))
    {
        HeapFree(GetProcessHeap(), 0, buffer);
        return NULL;
    }

    MultiByteToWideChar(CP_ACP, 0, start, end - start, target, chars);
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
    HeapFree(GetProcessHeap(), 0, resolved);

    TRACE("playback ended, hr %#lx\n", hr);
    return hr;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    const WCHAR *target;
    HRESULT hr;

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
