/*
 * Active Desktop tests
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#define COBJMACROS

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <shlguid.h>

#include "wine/test.h"

static const WCHAR components_keyW[] =
    L"Software\\Microsoft\\Internet Explorer\\Desktop\\Components";

static BOOL create_test_component(HKEY *key, DWORD *id)
{
    HKEY components;
    DWORD disposition, candidate;
    WCHAR name[16];
    LONG ret;
    unsigned int attempt;

    ret = RegCreateKeyExW(HKEY_CURRENT_USER, components_keyW, 0, NULL, 0,
                          KEY_CREATE_SUB_KEY | KEY_ENUMERATE_SUB_KEYS, NULL, &components, NULL);
    if (ret != ERROR_SUCCESS)
        return FALSE;

    candidate = 0x70000000u | (GetCurrentProcessId() & 0x0fffffff);
    for (attempt = 0; attempt < 256; ++attempt, ++candidate)
    {
        swprintf(name, ARRAY_SIZE(name), L"%lu", candidate);
        ret = RegCreateKeyExW(components, name, 0, NULL, 0, KEY_ALL_ACCESS,
                              NULL, key, &disposition);
        if (ret != ERROR_SUCCESS)
            break;
        if (disposition == REG_CREATED_NEW_KEY)
        {
            *id = candidate;
            RegCloseKey(components);
            return TRUE;
        }
        RegCloseKey(*key);
    }

    RegCloseKey(components);
    return FALSE;
}

static void delete_test_component(DWORD id)
{
    HKEY components;
    WCHAR name[16];

    if (RegOpenKeyExW(HKEY_CURRENT_USER, components_keyW, 0, KEY_WRITE, &components) != ERROR_SUCCESS)
        return;

    swprintf(name, ARRAY_SIZE(name), L"%lu", id);
    RegDeleteKeyW(components, name);
    RegCloseKey(components);
}

static void set_string(HKEY key, const WCHAR *name, const WCHAR *value)
{
    DWORD size = (lstrlenW(value) + 1) * sizeof(*value);
    LONG ret = RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value, size);
    ok(ret == ERROR_SUCCESS, "RegSetValueExW(%s) failed: %ld\n", wine_dbgstr_w(name), ret);
}

static void test_active_desktop_components(void)
{
    static const WCHAR sourceW[] = L"file:///C:/Windows/Web/whp-active-desktop-test.htm";
    static const WCHAR friendlyW[] = L"Water Active Desktop test";
    IActiveDesktop *desktop;
    COMPONENT component;
    COMPPOS position;
    HKEY key;
    DWORD id, type, current_state = IS_NORMAL;
    int before, after, i;
    HRESULT hr;
    BOOL found = FALSE;

    hr = CoCreateInstance(&CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER,
                          &IID_IActiveDesktop, (void **)&desktop);
    if (FAILED(hr))
    {
        win_skip("Active Desktop is unavailable, hr %#lx\n", hr);
        return;
    }

    hr = IActiveDesktop_GetDesktopItemCount(desktop, &before, 0);
    ok(hr == S_OK, "GetDesktopItemCount failed: %#lx\n", hr);
    if (FAILED(hr))
        before = 0;

    if (!create_test_component(&key, &id))
    {
        win_skip("Could not create an isolated Active Desktop component key\n");
        IActiveDesktop_Release(desktop);
        return;
    }

    type = COMP_TYPE_HTMLDOC;
    RegSetValueExW(key, L"Flags", 0, REG_DWORD, (const BYTE *)&type, sizeof(type));
    set_string(key, L"FriendlyName", friendlyW);
    set_string(key, L"Source", sourceW);
    set_string(key, L"SubscribedURL", sourceW);

    memset(&position, 0, sizeof(position));
    position.dwSize = sizeof(position);
    position.iLeft = 12;
    position.iTop = 34;
    position.dwWidth = 320;
    position.dwHeight = 200;
    position.izIndex = COMPONENT_TOP;
    position.fCanResize = TRUE;
    position.fCanResizeX = TRUE;
    position.fCanResizeY = TRUE;
    RegSetValueExW(key, L"Position", 0, REG_BINARY, (const BYTE *)&position, sizeof(position));
    RegSetValueExW(key, L"CurrentState", 0, REG_BINARY,
                   (const BYTE *)&current_state, sizeof(current_state));
    RegCloseKey(key);

    hr = IActiveDesktop_GetDesktopItemCount(desktop, &after, 0);
    ok(hr == S_OK, "GetDesktopItemCount after add failed: %#lx\n", hr);
    ok(after == before + 1, "expected %d components, got %d\n", before + 1, after);

    memset(&component, 0, sizeof(component));
    component.dwSize = sizeof(component);
    hr = IActiveDesktop_GetDesktopItemByID(desktop, id, &component, 0);
    ok(hr == S_OK, "GetDesktopItemByID failed: %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        ok(component.dwID == id, "expected id %#lx, got %#lx\n", id, component.dwID);
        ok(component.iComponentType == COMP_TYPE_HTMLDOC, "unexpected component type %d\n",
           component.iComponentType);
        ok(!lstrcmpW(component.wszFriendlyName, friendlyW), "unexpected friendly name %s\n",
           wine_dbgstr_w(component.wszFriendlyName));
        ok(!lstrcmpW(component.wszSource, sourceW), "unexpected source %s\n",
           wine_dbgstr_w(component.wszSource));
        ok(component.cpPos.dwSize == sizeof(COMPPOS), "unexpected COMPPOS size %lu\n",
           component.cpPos.dwSize);
        ok(component.cpPos.iLeft == 12 && component.cpPos.iTop == 34,
           "unexpected position %d,%d\n", component.cpPos.iLeft, component.cpPos.iTop);
        ok(component.dwCurItemState == IS_NORMAL, "unexpected item state %#lx\n",
           component.dwCurItemState);
    }

    {
        struct
        {
            IE4COMPONENT component;
            DWORD guard;
        } legacy;

        memset(&legacy, 0xcc, sizeof(legacy));
        legacy.component.dwSize = sizeof(legacy.component);
        legacy.guard = 0xdeadbeef;

        hr = IActiveDesktop_GetDesktopItemByID(desktop, id, (COMPONENT *)&legacy.component, 0);
        ok(hr == S_OK, "legacy GetDesktopItemByID failed: %#lx\n", hr);
        ok(legacy.guard == 0xdeadbeef, "IE4 component retrieval overwrote caller storage\n");
        ok(legacy.component.dwSize == sizeof(legacy.component), "unexpected IE4 size %lu\n",
           legacy.component.dwSize);
        ok(legacy.component.dwID == id, "unexpected IE4 id %#lx\n", legacy.component.dwID);
        ok(!lstrcmpW(legacy.component.wszSource, sourceW), "unexpected IE4 source %s\n",
           wine_dbgstr_w(legacy.component.wszSource));
    }

    memset(&component, 0, sizeof(component));
    component.dwSize = sizeof(component);
    hr = IActiveDesktop_GetDesktopItemBySource(desktop, sourceW, &component, 0);
    ok(hr == S_OK, "GetDesktopItemBySource failed: %#lx\n", hr);
    if (SUCCEEDED(hr))
        ok(component.dwID == id, "source lookup returned id %#lx instead of %#lx\n",
           component.dwID, id);

    for (i = 0; i < after; ++i)
    {
        memset(&component, 0, sizeof(component));
        component.dwSize = sizeof(component);
        if (SUCCEEDED(IActiveDesktop_GetDesktopItem(desktop, i, &component, 0)) &&
            component.dwID == id)
        {
            found = TRUE;
            break;
        }
    }
    ok(found, "test component was not reachable through index enumeration\n");

    delete_test_component(id);
    IActiveDesktop_Release(desktop);
}

START_TEST(activedesktop)
{
    HRESULT hr = CoInitialize(NULL);

    test_active_desktop_components();

    if (SUCCEEDED(hr))
        CoUninitialize();
}
