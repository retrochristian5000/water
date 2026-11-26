/*
 * NetShow launcher and event handling tests.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */
#include "wine/test.h"
#include "../nsplayer.h"

static void test_arguments(void)
{
    static const WCHAR *const empty[] = {L"nsplayer"};
    static const WCHAR *const local[] = {L"nsplayer", L"C:\\Media Files\\clip.asf"};
    static const WCHAR *const remote[] = {L"nsplayer", L"mms://example.invalid/clip.asf"};
    static const WCHAR *const help[] = {L"nsplayer", L"/?"};
    static const WCHAR *const blank[] = {L"nsplayer", L""};
    static const WCHAR *const extra[] = {L"nsplayer", L"clip.asf", L"other.asf"};

    ok(nsplayer_get_action(1, empty) == NSPLAYER_OPEN_DIALOG, "No argument should open file dialog\n");
    ok(nsplayer_get_action(2, local) == NSPLAYER_PLAY, "Local media target rejected\n");
    ok(nsplayer_get_action(2, remote) == NSPLAYER_PLAY, "Remote media target rejected\n");
    ok(nsplayer_get_action(2, help) == NSPLAYER_HELP, "Help not recognized\n");
    ok(nsplayer_get_action(2, blank) == NSPLAYER_INVALID, "Empty target accepted\n");
    ok(nsplayer_get_action(3, extra) == NSPLAYER_INVALID, "Additional argument ignored\n");
}

static void test_events(void)
{
    HRESULT hr = E_FAIL;
    BOOL terminal;

    terminal = nsplayer_terminal_event(EC_COMPLETE, 0, &hr);
    ok(terminal && hr == S_OK, "Completion: %d, %#lx\n", terminal, hr);
    terminal = nsplayer_terminal_event(EC_USERABORT, 0, &hr);
    ok(terminal && hr == E_ABORT, "Cancellation: %d, %#lx\n", terminal, hr);
    terminal = nsplayer_terminal_event(EC_ERRORABORT, E_ACCESSDENIED, &hr);
    ok(terminal && hr == E_ACCESSDENIED, "Error lost: %d, %#lx\n", terminal, hr);
    terminal = nsplayer_terminal_event(EC_ERRORABORT, S_OK, &hr);
    ok(terminal && hr == E_FAIL, "Invalid successful error event: %d, %#lx\n", terminal, hr);
    terminal = nsplayer_terminal_event(EC_REPAINT, 0, &hr);
    ok(!terminal && hr == E_FAIL, "Nonterminal event modified result: %d, %#lx\n", terminal, hr);
}

START_TEST(player)
{
    test_arguments();
    test_events();
}
