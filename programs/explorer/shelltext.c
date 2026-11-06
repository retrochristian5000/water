/*
 * Unicode CJK text drawing for Explorer UI surfaces.
 *
 * A Unicode string does not imply its selected GDI font has glyphs for
 * Chinese Hanzi or Japanese kanji/kana. Probe real font coverage and use
 * installed script-appropriate fonts without changing the source string.
 * Never synthesize font assets or assume a Chinese font has Japanese glyph
 * variants simply because Han code points overlap.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <windows.h>
#include <wchar.h>

#include "wine/debug.h"
#include "explorer_private.h"
#include "wine/win16_profile.h"

WINE_DEFAULT_DEBUG_CHANNEL(explorer);

enum cjk_script
{
    CJK_NONE,
    CJK_HAN,
    CJK_JAPANESE,
    CJK_KOREAN
};

static enum cjk_script detect_cjk(const WCHAR *text)
{
    BOOL han = FALSE, kana = FALSE, hangul = FALSE;
    const WCHAR *p;

    if (!text) return CJK_NONE;
    for (p = text; *p; p++)
    {
        WCHAR ch = *p;

        /* Extension B and later Han ideographs use UTF-16 surrogate
         * pairs. Detect them without slicing a code point in two. */
        if (ch >= 0xd800 && ch <= 0xdbff && p[1] >= 0xdc00 &&
            p[1] <= 0xdfff)
        {
            DWORD point = 0x10000 + (((DWORD)ch - 0xd800) << 10) +
                          ((DWORD)p[1] - 0xdc00);
            if (point >= 0x20000 && point <= 0x33479) han = TRUE;
            p++;
            continue;
        }

        if ((ch >= 0x3040 && ch <= 0x30ff) ||
            (ch >= 0x31f0 && ch <= 0x31ff) ||
            (ch >= 0xff66 && ch <= 0xff9f))
            kana = TRUE;
        if ((ch >= 0x3400 && ch <= 0x9fff) ||
            (ch >= 0xf900 && ch <= 0xfaff))
            han = TRUE;
        if ((ch >= 0x1100 && ch <= 0x11ff) ||
            (ch >= 0x3130 && ch <= 0x318f) ||
            (ch >= 0xac00 && ch <= 0xd7af))
            hangul = TRUE;
    }
    if (kana) return CJK_JAPANESE;
    if (hangul) return CJK_KOREAN;
    return han ? CJK_HAN : CJK_NONE;
}

BOOL shell_text_needs_cjk(const WCHAR *text)
{
    return detect_cjk(text) != CJK_NONE;
}

static BOOL is_traditional_chinese(LANGID locale)
{
    if (PRIMARYLANGID(locale) != LANG_CHINESE) return FALSE;
    switch (SUBLANGID(locale))
    {
    case SUBLANG_CHINESE_TRADITIONAL:
    case SUBLANG_CHINESE_HONGKONG:
    case SUBLANG_CHINESE_MACAU:
        return TRUE;
    }
    return FALSE;
}

/* Query glyph coverage of the selected font; returning GDI_ERROR is not a
 * successful coverage result. Query only BMP CJK chars, so a missing
 * supplementary-plane Han glyph is not falsely claimed as supported. */
static BOOL font_covers_text(HDC dc, const WCHAR *text)
{
    const WCHAR *p;

    for (p = text; *p; p++)
    {
        WORD glyph;
        DWORD result;

        /* GetGlyphIndicesW maps individual UTF-16 code units, not a
         * dependable supplementary-plane character shape. Do not claim
         * that a candidate font supports Extension B from this probe;
         * leave intact pairs to DrawTextW and the platform text engine. */
        if (*p >= 0xd800 && *p <= 0xdbff &&
            p[1] >= 0xdc00 && p[1] <= 0xdfff)
            return FALSE;

        if (!((*p >= 0x1100 && *p <= 0x11ff) ||
              (*p >= 0x3040 && *p <= 0x318f) ||
              (*p >= 0x3400 && *p <= 0x9fff) ||
              (*p >= 0xac00 && *p <= 0xd7af) ||
              (*p >= 0xf900 && *p <= 0xfaff) ||
              (*p >= 0xff66 && *p <= 0xff9f))) continue;

        result = GetGlyphIndicesW(dc, p, 1, &glyph, GGI_MARK_NONEXISTING_GLYPHS);
        if (result == GDI_ERROR || glyph == 0xffff) return FALSE;
    }
    return TRUE;
}

/* Probe only installed font faces; CreateFontIndirectW may silently
 * substitute another face, so compare the real selected family as well. */
static HFONT probe_font_families(HDC dc, const WCHAR *text, LOGFONTW *spec,
                                const WCHAR *const *families)
{
    UINT i;

    for (i = 0; families[i]; ++i)
    {
        HGDIOBJ previous;
        HFONT candidate;
        WCHAR face[LF_FACESIZE] = {0};
        BOOL matched;

        lstrcpynW(spec->lfFaceName, families[i], ARRAY_SIZE(spec->lfFaceName));
        candidate = CreateFontIndirectW(spec);
        if (!candidate) continue;

        previous = SelectObject(dc, candidate);
        if (!previous || previous == HGDI_ERROR)
        {
            DeleteObject(candidate);
            continue;
        }
        GetTextFaceW(dc, ARRAY_SIZE(face), face);
        matched = !lstrcmpiW(face, families[i]) && font_covers_text(dc, text);
        SelectObject(dc, previous);
        if (matched) return candidate;
        DeleteObject(candidate);
    }
    return NULL;
}

static HFONT find_cjk_font(HDC dc, const WCHAR *text, LANGID locale)
{
    static const WCHAR *const japanese[] =
    {
        L"Meiryo UI", L"Meiryo", L"Yu Gothic UI", L"MS UI Gothic",
        L"MS Gothic", L"Noto Sans CJK JP", NULL
    };
    static const WCHAR *const simplified[] =
    {
        L"Microsoft YaHei UI", L"Microsoft YaHei", L"SimSun",
        L"Noto Sans CJK SC", NULL
    };
    static const WCHAR *const traditional[] =
    {
        L"Microsoft JhengHei UI", L"Microsoft JhengHei", L"MingLiU",
        L"Noto Sans CJK TC", NULL
    };
    static const WCHAR *const korean[] =
    {
        L"Malgun Gothic", L"Gulim", L"Noto Sans CJK KR", NULL
    };
    static const WCHAR *const *const fallback[] =
    {
        japanese, traditional, simplified, korean
    };
    enum cjk_script script = detect_cjk(text);
    const WCHAR *const *faces;
    LOGFONTW spec;
    HGDIOBJ base;
    HFONT candidate;
    UINT i;
    BOOL known_locale;

    if (script == CJK_NONE) return NULL;
    if (!locale) locale = GetUserDefaultUILanguage();

    known_locale = PRIMARYLANGID(locale) == LANG_CHINESE ||
                   PRIMARYLANGID(locale) == LANG_JAPANESE ||
                   PRIMARYLANGID(locale) == LANG_KOREAN;

    /* Han-only text is ambiguous when the active locale is not CJK:
     * if the existing typeface covers it, preserve its glyph variant. */
    if (script == CJK_HAN && !known_locale && font_covers_text(dc, text))
        return NULL;

    if (script == CJK_JAPANESE ||
        (script == CJK_HAN && PRIMARYLANGID(locale) == LANG_JAPANESE))
        faces = japanese;
    else if (script == CJK_KOREAN ||
             (script == CJK_HAN && PRIMARYLANGID(locale) == LANG_KOREAN))
        faces = korean;
    else if (is_traditional_chinese(locale))
        faces = traditional;
    else
        faces = simplified;

    base = GetCurrentObject(dc, OBJ_FONT);
    if (!base || GetObjectW(base, sizeof(spec), &spec) != sizeof(spec))
        return NULL;
    spec.lfCharSet = DEFAULT_CHARSET;

    candidate = probe_font_families(dc, text, &spec, faces);
    if (candidate) return candidate;

    /* Let the installed fallback families rescue glyphs even if the
     * locale-specific face is missing; never invent an installed font. */
    if (!font_covers_text(dc, text))
    {
        for (i = 0; i < ARRAY_SIZE(fallback); ++i)
        {
            if (fallback[i] == faces) continue;
            candidate = probe_font_families(dc, text, &spec, fallback[i]);
            if (candidate) return candidate;
        }
    }
    return NULL;
}

/* Win9x is ANSI-first. Only explicit guest selection activates this path;
 * the Win16 profile's internal default must not affect an NT desktop. */
BOOL shell_text_is_win9x(void)
{
    char value[32];
    DWORD len;

    len = GetEnvironmentVariableA(WATER_WIN16_LINE_ENV, value, sizeof(value));
    if (len) return len < sizeof(value) && !strcmp(value, WATER_WIN16_LINE_WIN9X);

    len = GetEnvironmentVariableA(WATER_WIN16_PERSONALITY_ENV, value, sizeof(value));
    return len && len < sizeof(value) &&
           (!strcmp(value, WATER_WIN16_PERSONALITY_WIN95_OSR2) ||
            !strcmp(value, WATER_WIN16_PERSONALITY_WINME));
}

/* Conversion at the legacy shell boundary, using the same system ACP as
 * kernelbase/ntdll. Win9x will substitute for non-representable chars;
 * the NT path below preserves the original UTF-16 glyph sequence. */
static int draw_win9x_ansi_text(HDC dc, const WCHAR *text, RECT *rect, UINT flags)
{
    char stack[512], *bytes = stack;
    BOOL substituted = FALSE;
    int required, count, drawn;

    required = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
                                   text, -1, NULL, 0, NULL, NULL);
    if (!required) return 0;
    if (required > sizeof(stack))
    {
        bytes = HeapAlloc(GetProcessHeap(), 0, required);
        if (!bytes) return 0;
    }

    count = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, text, -1,
                                bytes, required, NULL, &substituted);
    if (!count)
    {
        if (bytes != stack) HeapFree(GetProcessHeap(), 0, bytes);
        return 0;
    }
    if (substituted)
        TRACE("Win9x ACP %u substituted unsupported characters in shell caption\n", GetACP());
    drawn = DrawTextA(dc, bytes, -1, rect, flags);
    if (bytes != stack) HeapFree(GetProcessHeap(), 0, bytes);
    return drawn;
}

int shell_draw_text(HDC dc, const WCHAR *text, RECT *rect, UINT flags, LANGID locale)
{
    HFONT fallback;
    HGDIOBJ previous = NULL;
    int result;

    if (!text) return 0;
    if (shell_text_is_win9x()) return draw_win9x_ansi_text(dc, text, rect, flags);
    fallback = find_cjk_font(dc, text, locale);
    if (fallback) previous = SelectObject(dc, fallback);
    result = DrawTextW(dc, text, -1, rect, flags);
    if (fallback)
    {
        if (previous && previous != HGDI_ERROR) SelectObject(dc, previous);
        DeleteObject(fallback);
    }
    return result;
}
