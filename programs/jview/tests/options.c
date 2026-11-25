/*
 * JVIEW launcher option tests
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "wine/test.h"
#include "../options.h"

static void test_options(void)
{
    static const WCHAR *const app[] = {
        L"/CP", L"C:\\Java classes", L"/cp:p", L"first.zip", L"/cp:a", L"last.zip",
        L"/d:name=some value", L"/d:empty=", L"/n", L"Example", L"/p", L"/v", L"/vst",
        L"Hello", L"/p", L"argument"
    };
    static const WCHAR *const applet[] = { L"/v", L"/a", L"page.html", L"/d:applet=value" };
    static const WCHAR *const help[] = { L"/?" };
    static const WCHAR *const missing_cp[] = { L"/cp" };
    static const WCHAR *const missing_class[] = { L"/v" };
    static const WCHAR *const missing_applet[] = { L"/a" };
    static const WCHAR *const invalid_property[] = { L"/d:=value", L"Hello" };
    static const WCHAR *const unknown[] = { L"/unknown", L"Hello" };
    static const WCHAR *const missing_value[] = { L"/cp", L"/v", L"Hello" };
    static const WCHAR *const no_property_value[] = { L"/d:key", L"Hello" };
    static const struct {
        int argc;
        const WCHAR *const *argv;
        enum jview_result expected;
    } cases[] = {
        {0, NULL, JVIEW_HELP},
        {ARRAY_SIZE(help), help, JVIEW_HELP},
        {ARRAY_SIZE(missing_cp), missing_cp, JVIEW_INVALID},
        {ARRAY_SIZE(missing_class), missing_class, JVIEW_INVALID},
        {ARRAY_SIZE(missing_applet), missing_applet, JVIEW_INVALID},
        {ARRAY_SIZE(invalid_property), invalid_property, JVIEW_INVALID},
        {ARRAY_SIZE(unknown), unknown, JVIEW_INVALID},
        {ARRAY_SIZE(missing_value), missing_value, JVIEW_INVALID},
        {ARRAY_SIZE(no_property_value), no_property_value, JVIEW_INVALID}
    };
    struct jview_options opts;
    enum jview_result result;
    unsigned int i;

    result = jview_parse(ARRAY_SIZE(app), app, &opts);
    ok(result == JVIEW_OK, "Application parse returned %d\n", result);
    ok(opts.target == 13, "Unexpected target index %d\n", opts.target);
    ok(opts.pause && opts.verify && opts.verbose && !opts.applet, "Unexpected flags\n");
    ok(opts.count == 6, "Unexpected option count %u\n", opts.count);
    if(opts.count == 6) {
        ok(opts.items[0].kind == JVIEW_CP_SET && !wcscmp(opts.items[0].value, app[1]), "Set path lost\n");
        ok(opts.items[1].kind == JVIEW_CP_PREPEND && !wcscmp(opts.items[1].value, app[3]), "Prepend path lost\n");
        ok(opts.items[2].kind == JVIEW_CP_APPEND && !wcscmp(opts.items[2].value, app[5]), "Append path lost\n");
        ok(opts.items[3].kind == JVIEW_PROPERTY && !wcscmp(opts.items[3].value, L"name=some value"), "Property lost\n");
        ok(opts.items[4].kind == JVIEW_PROPERTY && !wcscmp(opts.items[4].value, L"empty="), "Empty property lost\n");
        ok(opts.items[5].kind == JVIEW_NAMESPACE && !wcscmp(opts.items[5].value, L"Example"), "Namespace lost\n");
    }
    jview_free_options(&opts);

    result = jview_parse(ARRAY_SIZE(applet), applet, &opts);
    ok(result == JVIEW_OK, "Applet parse returned %d\n", result);
    ok(opts.applet && opts.verify && opts.target == 2 && !opts.count, "Applet arguments consumed as options\n");
    jview_free_options(&opts);

    for(i = 0; i < ARRAY_SIZE(cases); i++) {
        result = jview_parse(cases[i].argc, cases[i].argv, &opts);
        ok(result == cases[i].expected, "Case %u returned %d, expected %d\n", i, result, cases[i].expected);
        jview_free_options(&opts);
    }
}

START_TEST(options)
{
    test_options();
}
