/*
 * DOS KEYB compatibility command
 *
 * This is a clean-room command-layer implementation.  It preserves the
 * classic KEYB command shape and records DOS keyboard state for winevdm.
 * Actual scan-code translation is currently supplied by the DOS backend.
 */

#include <windows.h>
#include <stdio.h>
#include <wchar.h>
#include <wctype.h>

#include "wine/doskeyb.h"

struct keyb_state
{
    WCHAR layout[WINE_DOS_KEYB_MAX_LAYOUT + 1];
    WCHAR id[WINE_DOS_KEYB_MAX_ID + 1];
    WCHAR definition[MAX_PATH];
    DWORD codepage;
    BOOL have_codepage;
    BOOL enhanced;
};

static void usage(void)
{
    wprintf(L"Loads a DOS keyboard layout.\n\n"
            L"KEYB [xx[,[yyy],[pathname]]] [/ID:nnn] [/E]\n\n"
            L"  xx        Two-letter keyboard code.\n"
            L"  yyy       DOS code page.\n"
            L"  pathname  Keyboard definition file.\n"
            L"  /ID:nnn   Keyboard identifier.\n"
            L"  /E        Request enhanced-keyboard handling.\n");
}

static BOOL parse_decimal(const WCHAR *str, DWORD limit, DWORD *value)
{
    DWORD result = 0;

    if (!str || !*str) return FALSE;
    while (*str)
    {
        DWORD digit;

        if (*str < L'0' || *str > L'9') return FALSE;
        digit = *str++ - L'0';
        if (result > (limit - digit) / 10) return FALSE;
        result = result * 10 + digit;
    }

    if (!result) return FALSE;
    *value = result;
    return TRUE;
}

static WCHAR ascii_upper(WCHAR ch)
{
    if (ch >= L'a' && ch <= L'z') return ch - (L'a' - L'A');
    return ch;
}

static BOOL ascii_equal_nocase(const WCHAR *left, const WCHAR *right)
{
    while (*left && *right)
    {
        if (ascii_upper(*left++) != ascii_upper(*right++)) return FALSE;
    }
    return !*left && !*right;
}

static BOOL ascii_prefix_nocase(const WCHAR *str, const WCHAR *prefix)
{
    while (*prefix)
    {
        if (!*str || ascii_upper(*str++) != ascii_upper(*prefix++)) return FALSE;
    }
    return TRUE;
}

static BOOL valid_layout(const WCHAR *layout)
{
    unsigned int len = 0;

    while (layout[len])
    {
        if (!iswalpha(layout[len]) || len >= WINE_DOS_KEYB_MAX_LAYOUT) return FALSE;
        len++;
    }
    return len == 2;
}

static void uppercase_layout(WCHAR *layout)
{
    unsigned int i;

    for (i = 0; layout[i]; i++) layout[i] = towupper(layout[i]);
}

static BOOL parse_spec(const WCHAR *arg, struct keyb_state *state)
{
    WCHAR buffer[MAX_PATH + 32], *field, *next;
    unsigned int index = 0;

    if (wcslen(arg) >= ARRAY_SIZE(buffer)) return FALSE;
    wcscpy(buffer, arg);

    field = buffer;
    for (;;)
    {
        next = wcschr(field, ',');
        if (next) *next++ = 0;

        switch (index)
        {
        case 0:
            if (!valid_layout(field)) return FALSE;
            wcscpy(state->layout, field);
            uppercase_layout(state->layout);
            break;

        case 1:
            if (*field)
            {
                DWORD value;

                if (!parse_decimal(field, 65535, &value)) return FALSE;
                state->codepage = value;
                state->have_codepage = TRUE;
            }
            break;

        case 2:
            if (*field)
            {
                if (wcslen(field) >= ARRAY_SIZE(state->definition)) return FALSE;
                wcscpy(state->definition, field);
            }
            break;

        default:
            return FALSE;
        }

        index++;
        if (!next) break;
        field = next;
    }

    return TRUE;
}

static BOOL read_string(HKEY key, const WCHAR *name, WCHAR *buffer, DWORD count)
{
    DWORD type, size = count * sizeof(*buffer);
    LSTATUS status = RegQueryValueExW(key, name, NULL, &type, (BYTE *)buffer, &size);

    if (status != ERROR_SUCCESS || type != REG_SZ || !size)
        return FALSE;

    buffer[count - 1] = 0;
    return TRUE;
}

static BOOL read_state(struct keyb_state *state)
{
    HKEY key;
    DWORD type, size;
    LSTATUS status;

    ZeroMemory(state, sizeof(*state));

    status = RegOpenKeyExW(HKEY_CURRENT_USER, WINE_DOS_KEYB_REGKEY, 0, KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS) return FALSE;

    if (!read_string(key, WINE_DOS_KEYB_LAYOUT_VALUE, state->layout, ARRAY_SIZE(state->layout)) ||
        !valid_layout(state->layout))
    {
        RegCloseKey(key);
        return FALSE;
    }

    uppercase_layout(state->layout);

    size = sizeof(state->codepage);
    if (RegQueryValueExW(key, WINE_DOS_KEYB_CODEPAGE_VALUE, NULL, &type,
                         (BYTE *)&state->codepage, &size) == ERROR_SUCCESS &&
        type == REG_DWORD && size == sizeof(state->codepage) &&
        state->codepage && state->codepage <= 65535)
        state->have_codepage = TRUE;

    read_string(key, WINE_DOS_KEYB_FILE_VALUE, state->definition, ARRAY_SIZE(state->definition));
    read_string(key, WINE_DOS_KEYB_ID_VALUE, state->id, ARRAY_SIZE(state->id));

    size = sizeof(state->enhanced);
    if (RegQueryValueExW(key, WINE_DOS_KEYB_ENHANCED_VALUE, NULL, &type,
                         (BYTE *)&state->enhanced, &size) != ERROR_SUCCESS ||
        type != REG_DWORD || size != sizeof(state->enhanced))
        state->enhanced = FALSE;
    else
        state->enhanced = !!state->enhanced;

    RegCloseKey(key);
    return TRUE;
}

static BOOL set_or_delete_string(HKEY key, const WCHAR *name, const WCHAR *value)
{
    LSTATUS status;

    if (!*value)
    {
        status = RegDeleteValueW(key, name);
        return status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    status = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value,
                            (wcslen(value) + 1) * sizeof(*value));
    return status == ERROR_SUCCESS;
}

static BOOL write_state(const struct keyb_state *state)
{
    HKEY key;
    DWORD enhanced = state->enhanced;
    LSTATUS status;
    BOOL ok = TRUE;

    status = RegCreateKeyExW(HKEY_CURRENT_USER, WINE_DOS_KEYB_REGKEY, 0, NULL, 0,
                             KEY_SET_VALUE, NULL, &key, NULL);
    if (status != ERROR_SUCCESS) return FALSE;

    ok &= set_or_delete_string(key, WINE_DOS_KEYB_LAYOUT_VALUE, state->layout);
    ok &= set_or_delete_string(key, WINE_DOS_KEYB_FILE_VALUE, state->definition);
    ok &= set_or_delete_string(key, WINE_DOS_KEYB_ID_VALUE, state->id);

    if (state->have_codepage)
        ok &= RegSetValueExW(key, WINE_DOS_KEYB_CODEPAGE_VALUE, 0, REG_DWORD,
                             (const BYTE *)&state->codepage, sizeof(state->codepage)) == ERROR_SUCCESS;
    else
    {
        status = RegDeleteValueW(key, WINE_DOS_KEYB_CODEPAGE_VALUE);
        ok &= status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    if (state->enhanced)
        ok &= RegSetValueExW(key, WINE_DOS_KEYB_ENHANCED_VALUE, 0, REG_DWORD,
                             (const BYTE *)&enhanced, sizeof(enhanced)) == ERROR_SUCCESS;
    else
    {
        status = RegDeleteValueW(key, WINE_DOS_KEYB_ENHANCED_VALUE);
        ok &= status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }

    RegCloseKey(key);
    return ok;
}

static void query_state(void)
{
    struct keyb_state state;

    if (!read_state(&state))
    {
        wprintf(L"KEYB is not configured for Water DOS sessions.\n");
        return;
    }

    wprintf(L"Keyboard code: %ls\n", state.layout);
    if (*state.id) wprintf(L"Keyboard ID: %ls\n", state.id);
    if (state.have_codepage) wprintf(L"Code page: %lu\n", state.codepage);
    else wprintf(L"Code page: automatic\n");
    if (*state.definition) wprintf(L"Definition file: %ls\n", state.definition);
    if (state.enhanced) wprintf(L"Enhanced keyboard: yes\n");
}

int __cdecl wmain(int argc, WCHAR **argv)
{
    struct keyb_state state;
    const WCHAR *spec = NULL;
    int i;

    ZeroMemory(&state, sizeof(state));

    if (argc == 1)
    {
        query_state();
        return 0;
    }

    for (i = 1; i < argc; i++)
    {
        const WCHAR *arg = argv[i];

        if (!wcscmp(arg, L"/?") || !wcscmp(arg, L"-?"))
        {
            usage();
            return 0;
        }
        if (ascii_equal_nocase(arg, L"/E"))
        {
            state.enhanced = TRUE;
            continue;
        }
        if (ascii_prefix_nocase(arg, L"/ID:"))
        {
            const WCHAR *id = arg + 4;
            DWORD id_value;

            if (wcslen(id) > WINE_DOS_KEYB_MAX_ID ||
                !parse_decimal(id, 999, &id_value))
            {
                fwprintf(stderr, L"KEYB: invalid keyboard ID.\n");
                return 1;
            }
            wcscpy(state.id, id);
            continue;
        }
        if (arg[0] == L'/' || arg[0] == L'-' || spec)
        {
            fwprintf(stderr, L"KEYB: invalid parameter.\n");
            return 1;
        }
        spec = arg;
    }

    if (!spec || !parse_spec(spec, &state))
    {
        fwprintf(stderr, L"KEYB: invalid keyboard specification.\n");
        return 1;
    }

    if (!write_state(&state))
    {
        fwprintf(stderr, L"KEYB: unable to save DOS keyboard state.\n");
        return 1;
    }

    return 0;
}
