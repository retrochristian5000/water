/*
 * ieframe - Internet Explorer main frame window
 *
 * Copyright 2006 Mike McCormack (for CodeWeavers)
 * Copyright 2006 Jacek Caban (for CodeWeavers)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#define COBJMACROS

#include <stdarg.h>
#include <string.h>

#include "ieframe.h"
#include "ocidl.h"
#include "winreg.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ieframe);


struct browser_helper
{
    struct list entry;
    IObjectWithSite *site;
};

static BOOL read_reg_dword(HKEY root, const WCHAR *path, const WCHAR *name, DWORD *value)
{
    DWORD type, size = sizeof(*value);
    HKEY key;
    LONG err;

    if (RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return FALSE;
    err = RegQueryValueExW(key, name, NULL, &type, (BYTE *)value, &size);
    RegCloseKey(key);
    return err == ERROR_SUCCESS && type == REG_DWORD && size == sizeof(*value);
}

/* Returns -1 for no recognized setting, 0 to disable and 1 to enable. */
static int get_extension_setting(HKEY root, const WCHAR *path)
{
    WCHAR value[16];
    DWORD type, size = sizeof(value);
    HKEY key;
    LONG err;

    if (RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return -1;
    err = RegQueryValueExW(key, L"Enable Browser Extensions", NULL, &type,
            (BYTE *)value, &size);
    RegCloseKey(key);
    if (err != ERROR_SUCCESS || type != REG_SZ || size < sizeof(WCHAR)
            || size > sizeof(value) || size % sizeof(WCHAR))
        return -1;

    /* RegQueryValueExW does not guarantee a terminating NUL. */
    value[size < sizeof(value) ? size / sizeof(WCHAR) : ARRAY_SIZE(value) - 1] = 0;
    if (!lstrcmpiW(value, L"No"))
        return 0;
    if (!lstrcmpiW(value, L"Yes"))
        return 1;
    return -1;
}

static BOOL browser_extensions_enabled(void)
{
    static const WCHAR main_key[] = L"Software\\Microsoft\\Internet Explorer\\Main";
    static const WCHAR policy_key[] = L"Software\\Policies\\Microsoft\\Internet Explorer\\Main";
    int result;

    /* An administrator policy takes precedence over user preferences. */
    result = get_extension_setting(HKEY_LOCAL_MACHINE, policy_key);
    if (result == -1)
        result = get_extension_setting(HKEY_CURRENT_USER, policy_key);
    if (result == -1)
        result = get_extension_setting(HKEY_CURRENT_USER, main_key);
    return result != 0;
}

static int browser_helper_policy(HKEY root, const WCHAR *clsid)
{
    static const WCHAR path[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Ext\\CLSID";
    WCHAR value[16];
    DWORD number, type, size = sizeof(value);
    HKEY key;
    LONG err;

    if (RegOpenKeyExW(root, path, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return -1;
    err = RegQueryValueExW(key, clsid, NULL, &type, (BYTE *)value, &size);
    RegCloseKey(key);
    if (err != ERROR_SUCCESS)
        return -1;

    if (type == REG_DWORD && size == sizeof(number))
    {
        memcpy(&number, value, sizeof(number));
        return number <= 2 ? number : -1;
    }
    if (type == REG_SZ && size >= sizeof(WCHAR) && size <= sizeof(value))
    {
        if (size % sizeof(WCHAR))
            return -1;
        value[size < sizeof(value) ? size / sizeof(WCHAR) : ARRAY_SIZE(value) - 1] = 0;
        if (!lstrcmpW(value, L"0")) return 0;
        if (!lstrcmpW(value, L"1")) return 1;
        if (!lstrcmpW(value, L"2")) return 2;
    }
    return -1;
}

static BOOL browser_helper_allowed(const WCHAR *clsid)
{
    static const WCHAR ext_policy_key[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Ext";
    static const WCHAR settings_prefix[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Ext\\Settings\\";
    WCHAR settings[ARRAY_SIZE(settings_prefix) + 64];
    HKEY roots[] = {HKEY_LOCAL_MACHINE, HKEY_CURRENT_USER};
    DWORD flags, restrict_list;
    BOOL allowed = FALSE, restricted = FALSE;
    unsigned int i;
    int rule;

    for (i = 0; i < ARRAY_SIZE(roots); i++)
    {
        rule = browser_helper_policy(roots[i], clsid);
        if (rule == 0)
            return FALSE;
        if (rule == 1 || rule == 2)
            allowed = TRUE;
        if (read_reg_dword(roots[i], ext_policy_key, L"RestrictToList", &restrict_list)
                && restrict_list)
            restricted = TRUE;
    }
    if (restricted && !allowed)
        return FALSE;

    if (lstrlenW(clsid) >= 64)
        return FALSE;
    lstrcpyW(settings, settings_prefix);
    lstrcatW(settings, clsid);
    if (read_reg_dword(HKEY_CURRENT_USER, settings, L"Flags", &flags) && (flags & 1))
        return FALSE;
    return TRUE;
}

void release_browser_helpers(InternetExplorer *ie)
{
    struct browser_helper *helper;

    /* SetSite(NULL) can invoke Quit or the browser's release path again. */
    ie->bho_active = FALSE;
    if (ie->bho_releasing)
        return;

    ie->bho_releasing = TRUE;
    while (!list_empty(&ie->bhos))
    {
        helper = LIST_ENTRY(ie->bhos.next, struct browser_helper, entry);
        list_remove(&helper->entry);
        IObjectWithSite_SetSite(helper->site, NULL);
        IObjectWithSite_Release(helper->site);
        free(helper);
    }
    ie->bho_releasing = FALSE;
}

void load_browser_helpers(InternetExplorer *ie)
{
    static const WCHAR registry_key[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Browser Helper Objects";
    WCHAR name[64];
    DWORD index;
    LONG err;
    HKEY key;

    if (ie->bho_active || ie->bho_loading || ie->bho_releasing || !browser_extensions_enabled())
        return;
    ie->bho_active = TRUE;
    ie->bho_loading = TRUE;

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, registry_key, 0, KEY_ENUMERATE_SUB_KEYS, &key)
            == ERROR_SUCCESS)
    {
        for (index = 0; ie->bho_active; index++)
        {
            struct browser_helper *helper;
            IObjectWithSite *site;
            DWORD name_len = ARRAY_SIZE(name);
            CLSID clsid;
            HRESULT hr;

            err = RegEnumKeyExW(key, index, name, &name_len, NULL, NULL, NULL, NULL);
            if (err == ERROR_NO_MORE_ITEMS)
                break;
            if (err != ERROR_SUCCESS || FAILED(CLSIDFromString(name, &clsid))
                    || !browser_helper_allowed(name))
                continue;

            hr = CoCreateInstance(&clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IObjectWithSite,
                    (void **)&site);
            if (FAILED(hr))
            {
                WARN("Could not load browser helper %s: %#lx\n", debugstr_w(name), hr);
                continue;
            }

            hr = IObjectWithSite_SetSite(site, (IUnknown *)&ie->IWebBrowser2_iface);
            if (SUCCEEDED(hr) && ie->bho_active && (helper = malloc(sizeof(*helper))))
            {
                helper->site = site;
                list_add_tail(&ie->bhos, &helper->entry);
            }
            else
            {
                if (SUCCEEDED(hr))
                    IObjectWithSite_SetSite(site, NULL);
                IObjectWithSite_Release(site);
            }
        }
        RegCloseKey(key);
    }
    ie->bho_loading = FALSE;
}
