/*
 * JVIEW command-line compatibility
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef WATER_JVIEW_OPTIONS_H
#define WATER_JVIEW_OPTIONS_H

#include <stdlib.h>
#include <wchar.h>
#include "windef.h"

enum jview_result { JVIEW_OK, JVIEW_HELP, JVIEW_INVALID, JVIEW_NOMEM };
enum jview_option_kind { JVIEW_CP_SET, JVIEW_CP_PREPEND, JVIEW_CP_APPEND, JVIEW_PROPERTY, JVIEW_NAMESPACE };

struct jview_option
{
    enum jview_option_kind kind;
    const WCHAR *value;
};

struct jview_options
{
    BOOL applet, pause, verify, verbose;
    unsigned int count;
    struct jview_option *items;
    int target;
    const WCHAR *error;
};

static inline BOOL jview_equal(const WCHAR *a, const WCHAR *b)
{
    return !wcsicmp(a, b);
}

static inline void jview_free_options(struct jview_options *opts)
{
    free(opts->items);
    opts->items = NULL;
    opts->count = 0;
}

/* argv excludes the executable. Preserve argument order and spelling;
 * classpath operations and properties are resolved by the future VM adapter.
 * Once a class or /a is reached, the remaining tokens are not launcher flags. */
static inline enum jview_result jview_parse(int argc, const WCHAR *const *argv,
        struct jview_options *opts)
{
    const WCHAR *arg, *value, *equals;
    enum jview_option_kind kind;
    int i;

    *opts = (struct jview_options){0};
    opts->target = argc;
    if(!argc)
        return JVIEW_HELP;

    if(!(opts->items = calloc(argc, sizeof(*opts->items))))
        return JVIEW_NOMEM;

    for(i = 0; i < argc; i++) {
        arg = argv[i];
        if(arg[0] != '/') {
            if(!arg[0])
                goto invalid;
            opts->target = i;
            return JVIEW_OK;
        }

        if(jview_equal(arg, L"/?"))
            return JVIEW_HELP;
        if(jview_equal(arg, L"/a")) {
            opts->applet = TRUE;
            if(i + 1 == argc)
                goto invalid;
            opts->target = i + 1;
            return JVIEW_OK;
        }
        if(jview_equal(arg, L"/p")) {
            opts->pause = TRUE;
            continue;
        }
        if(jview_equal(arg, L"/v")) {
            opts->verify = TRUE;
            continue;
        }
        if(jview_equal(arg, L"/vst")) {
            opts->verbose = TRUE;
            continue;
        }
        if(!wcsnicmp(arg, L"/d:", 3)) {
            value = arg + 3;
            equals = wcschr(value, '=');
            if(!equals || equals == value)
                goto invalid;
            kind = JVIEW_PROPERTY;
        }else {
            if(jview_equal(arg, L"/cp"))
                kind = JVIEW_CP_SET;
            else if(jview_equal(arg, L"/cp:p"))
                kind = JVIEW_CP_PREPEND;
            else if(jview_equal(arg, L"/cp:a"))
                kind = JVIEW_CP_APPEND;
            else if(jview_equal(arg, L"/n"))
                kind = JVIEW_NAMESPACE;
            else
                goto invalid;

            if(i + 1 == argc || argv[i + 1][0] == '/' || !argv[i + 1][0])
                goto invalid;
            value = argv[++i];
        }
        opts->items[opts->count++] = (struct jview_option){kind, value};
    }

    opts->error = L"Missing Java class or applet.";
    return JVIEW_INVALID;

invalid:
    opts->error = arg;
    return JVIEW_INVALID;
}

#endif
