/*
 * NetShow launcher and playback event handling.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#ifndef WATER_NSPLAYER_H
#define WATER_NSPLAYER_H

#include <windows.h>
#include <dshow.h>

enum nsplayer_action { NSPLAYER_OPEN_DIALOG, NSPLAYER_PLAY, NSPLAYER_HELP, NSPLAYER_INVALID };

/* One media location is historically documented. Help and argument
 * validation are WATER's current behavior, not verified native switches. */
static inline enum nsplayer_action nsplayer_get_action(int argc, const WCHAR *const *argv)
{
    if (argc == 1) return NSPLAYER_OPEN_DIALOG;
    if (argc != 2 || !argv[1][0]) return NSPLAYER_INVALID;
    if (!lstrcmpW(argv[1], L"/?")) return NSPLAYER_HELP;
    return NSPLAYER_PLAY;
}

static inline BOOL nsplayer_terminal_event(LONG code, LONG_PTR param, HRESULT *hr)
{
    switch (code)
    {
    case EC_COMPLETE:
        *hr = S_OK;
        return TRUE;
    case EC_USERABORT:
        *hr = E_ABORT;
        return TRUE;
    case EC_ERRORABORT:
        *hr = (HRESULT)param;
        if (SUCCEEDED(*hr)) *hr = E_FAIL;
        return TRUE;
    default:
        return FALSE;
    }
}
#endif
