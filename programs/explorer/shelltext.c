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
    enum cjk_script script = detect_cjk(text);
    const WCHAR *const *faces;
    LOGFONTW spec;
    HGDIOBJ base, previous;
    HFONT candidate;
    WCHAR selected_name[LF_FACESIZE];
    UINT i;

    if (script == CJK_NONE) return NULL;
    if (!locale) locale = GetUserDefaultUILanguage();

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
    if (!base || GetObjectW(base, sizeof(spec), &spec) != sizeof(spec)) return NULL;
    spec.lfCharSet = DEFAULT_CHARSET;

    for (i = 0; faces[i]; i++)
    {
        lstrcpynW(spec.lfFaceName, faces[i], ARRAY_SIZE(spec.lfFaceName));
        candidate = CreateFontIndirectW(&spec);
        if (!candidate) continue;

        previous = SelectObject(dc, candidate);
        if (!previous || previous == HGDI_ERROR)
        {
            DeleteObject(candidate);
            continue;
        }
        selected_name[0] = 0;
        GetTextFaceW(dc, ARRAY_SIZE(selected_name), selected_name);
        if (!lstrcmpiW(selected_name, faces[i]) && font_covers_text(dc, text))
        {
            SelectObject(dc, previous);
            return candidate;
        }
        SelectObject(dc, previous);
        DeleteObject(candidate);
    }

    /* A different locale may supply the only installed CJK font. Attempt
     * fallback families without misrepresenting native glyph provenance. */
    if (script == CJK_HAN && !font_covers_text(dc, text))
    {
        const WCHAR *const *other[] = {japanese, traditional, simplified};
        UINT j;
        for (j = 0; j < ARRAY_SIZE(other); j++)
        {
            for (i = 0; other[j][i]; i++)
            {
                lstrcpynW(spec.lfFaceName, other[j][i], ARRAY_SIZE(spec.lfFaceName));
                candidate = CreateFontIndirectW(&spec);
                if (!candidate) continue;
                previous = SelectObject(dc, candidate);
                if (!previous || previous == HGDI_ERROR)
                {
                    DeleteObject(candidate);
                    continue;
                }
                selected_name[0] = 0;
                GetTextFaceW(dc, ARRAY_SIZE(selected_name), selected_name);
                if (!lstrcmpiW(selected_name, other[j][i]) &&
                    font_covers_text(dc, text))
                {
                    SelectObject(dc, previous);
                    return candidate;
                }
                SelectObject(dc, previous);
                DeleteObject(candidate);
            }
        }
    }
    return NULL;
}

int shell_draw_text(HDC dc, const WCHAR *text, RECT *rect, UINT flags, LANGID locale)
{
    HFONT fallback;
    HGDIOBJ previous = NULL;
    int result;

    if (!text) return 0;
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
