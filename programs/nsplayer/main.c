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

static HRESULT play_target(const WCHAR *target)
{
    IMediaControl *control = NULL;
    IMediaEvent *event = NULL;
    BSTR filename;
    LONG event_code = 0;
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
        hr = IMediaEvent_WaitForCompletion(event, INFINITE, &event_code);

    IMediaControl_Stop(control);
    IMediaEvent_Release(event);
    IMediaControl_Release(control);

    TRACE("playback ended with event %ld, hr %#lx\n", event_code, hr);
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
