/*
 * JVIEW command-line compatibility
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#include <stdio.h>
#include "windef.h"
#include "winbase.h"
#include "wincon.h"
#include "options.h"

static void usage(void)
{
    puts("WATER JVIEW - Java command-line compatibility launcher");
    puts("Usage: jview [options] classname [arguments]");
    puts("  /?                 Show help");
    puts("  /cp path           Replace class path");
    puts("  /cp:p path         Prepend class path");
    puts("  /cp:a path         Append class path");
    puts("  /d:name=value      Define system property");
    puts("  /n namespace       Select namespace");
    puts("  /p                 Pause on error");
    puts("  /v                 Request verification of all classes");
    puts("  /vst               Request detailed stack traces");
    puts("  /a target          Select applet viewer");
    puts("No Microsoft-compatible Java execution backend is implemented.");
}

static void pause_on_error(void)
{
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode, count;
    WCHAR line[2];

    /* Avoid waiting on redirected stdin or unattended pipelines. */
    if(GetConsoleMode(input, &mode) && (mode & ENABLE_LINE_INPUT)) {
        fputs("Press Enter to continue...\n", stderr);
        fflush(stderr);
        ReadConsoleW(input, line, ARRAY_SIZE(line), &count, NULL);
    }
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    struct jview_options opts;
    enum jview_result result;
    int ret = 1;

    result = jview_parse(argc - 1, (const WCHAR *const *)(argv + 1), &opts);
    switch(result) {
    case JVIEW_HELP:
        usage();
        ret = 0;
        break;
    case JVIEW_INVALID:
        fwprintf(stderr, L"JVIEW: invalid or incomplete command: %ls\n", opts.error);
        break;
    case JVIEW_NOMEM:
        fputs("JVIEW: insufficient memory.\n", stderr);
        break;
    case JVIEW_OK:
        /* Never imply successful execution or load an unverified legacy VM. */
        fputs("JVIEW: Microsoft-compatible Java execution backend unavailable.\n", stderr);
        break;
    }
    if(ret && opts.pause)
        pause_on_error();
    jview_free_options(&opts);
    return ret;
}
