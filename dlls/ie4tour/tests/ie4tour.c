/*
 * Tests for the Internet Explorer 4 Welcome Tour compatibility component.
 */

#define COBJMACROS

#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "objbase.h"
#include "oleauto.h"
#include "objsafe.h"

#include "wine/test.h"

static const GUID CLSID_RunOnceCheckBox =
    {0xd81a5542,0x0f4e,0x11d1,{0x96,0x71,0x00,0xa0,0xc9,0x05,0x41,0x68}};

#define EXPLORER_TIPS_KEY "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Tips"
#define SHOW_IE4_VALUE    "ShowIE4"

static HRESULT (WINAPI *pDllCanUnloadNow)(void);
static HRESULT (WINAPI *pDllGetClassObject)(REFCLSID, REFIID, void **);
static HRESULT (WINAPI *pDllRegisterServer)(void);
static HRESULT (WINAPI *pDllUnregisterServer)(void);

static void test_exports(HMODULE module)
{
    pDllCanUnloadNow = (void *)GetProcAddress(module, "DllCanUnloadNow");
    pDllGetClassObject = (void *)GetProcAddress(module, "DllGetClassObject");
    pDllRegisterServer = (void *)GetProcAddress(module, "DllRegisterServer");
    pDllUnregisterServer = (void *)GetProcAddress(module, "DllUnregisterServer");

    ok(pDllCanUnloadNow != NULL, "DllCanUnloadNow is missing\n");
    ok(pDllGetClassObject != NULL, "DllGetClassObject is missing\n");
    ok(pDllRegisterServer != NULL, "DllRegisterServer is missing\n");
    ok(pDllUnregisterServer != NULL, "DllUnregisterServer is missing\n");

    ok(GetProcAddress(module, MAKEINTRESOURCEA(1)) == (FARPROC)pDllCanUnloadNow,
       "ordinal 1 does not match DllCanUnloadNow\n");
    ok(GetProcAddress(module, MAKEINTRESOURCEA(2)) == (FARPROC)pDllGetClassObject,
       "ordinal 2 does not match DllGetClassObject\n");
    ok(GetProcAddress(module, MAKEINTRESOURCEA(3)) == (FARPROC)pDllRegisterServer,
       "ordinal 3 does not match DllRegisterServer\n");
    ok(GetProcAddress(module, MAKEINTRESOURCEA(4)) == (FARPROC)pDllUnregisterServer,
       "ordinal 4 does not match DllUnregisterServer\n");
}

static void restore_show_ie4(HKEY key, BOOL existed, DWORD type, const BYTE *data, DWORD size)
{
    if (existed)
        RegSetValueExA(key, SHOW_IE4_VALUE, 0, type, data, size);
    else
        RegDeleteValueA(key, SHOW_IE4_VALUE);
}

static void test_runonce_object(void)
{
    IClassFactory *factory;
    IObjectSafety *safety;
    IDispatch *dispatch;
    ITypeInfo *typeinfo;
    DISPID dispid;
    OLECHAR *name = L"ShowIE4State";
    DWORD supported, enabled;
    HKEY key;
    BYTE *old_data = NULL;
    DWORD old_type = 0, old_size = 0, zero = 0;
    BOOL old_exists = FALSE, key_created = FALSE;
    HRESULT hr;
    UINT count;
    VARIANT result, arg;
    DISPPARAMS params;
    DISPID putid = DISPID_PROPERTYPUT;
    LSTATUS status;

    hr = pDllGetClassObject(&CLSID_RunOnceCheckBox, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK, "DllGetClassObject returned %#lx\n", hr);
    if (FAILED(hr)) return;

    ok(pDllCanUnloadNow() == S_FALSE, "class factory did not lock the module\n");

    hr = IClassFactory_CreateInstance(factory, NULL, &IID_IDispatch, (void **)&dispatch);
    ok(hr == S_OK, "CreateInstance returned %#lx\n", hr);
    IClassFactory_Release(factory);
    if (FAILED(hr)) return;

    hr = IDispatch_GetTypeInfoCount(dispatch, &count);
    ok(hr == S_OK, "GetTypeInfoCount returned %#lx\n", hr);
    ok(count == 1, "unexpected typeinfo count %u\n", count);

    hr = IDispatch_GetTypeInfo(dispatch, 0, LOCALE_SYSTEM_DEFAULT, &typeinfo);
    ok(hr == S_OK, "GetTypeInfo returned %#lx\n", hr);
    if (SUCCEEDED(hr)) ITypeInfo_Release(typeinfo);

    hr = IDispatch_GetIDsOfNames(dispatch, &IID_NULL, &name, 1, LOCALE_SYSTEM_DEFAULT, &dispid);
    ok(hr == S_OK, "GetIDsOfNames returned %#lx\n", hr);
    ok(dispid == 1, "ShowIE4State has unexpected dispid %ld\n", dispid);

    hr = IDispatch_QueryInterface(dispatch, &IID_IObjectSafety, (void **)&safety);
    ok(hr == S_OK, "IObjectSafety query returned %#lx\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IObjectSafety_GetInterfaceSafetyOptions(safety, &IID_IDispatch, &supported, &enabled);
        ok(hr == S_OK, "GetInterfaceSafetyOptions returned %#lx\n", hr);
        ok((supported & (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA)) ==
           (INTERFACESAFE_FOR_UNTRUSTED_CALLER | INTERFACESAFE_FOR_UNTRUSTED_DATA),
           "unexpected supported safety flags %#lx\n", supported);
        ok((enabled & supported) == supported, "unexpected enabled safety flags %#lx\n", enabled);
        IObjectSafety_Release(safety);
    }

    status = RegOpenKeyExA(HKEY_CURRENT_USER, EXPLORER_TIPS_KEY, 0,
                           KEY_QUERY_VALUE | KEY_SET_VALUE, &key);
    if (status == ERROR_FILE_NOT_FOUND)
    {
        status = RegCreateKeyExA(HKEY_CURRENT_USER, EXPLORER_TIPS_KEY, 0, NULL, 0,
                                 KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &key, NULL);
        key_created = status == ERROR_SUCCESS;
    }
    ok(status == ERROR_SUCCESS, "opening Explorer\\Tips returned %ld\n", status);
    if (status == ERROR_SUCCESS)
    {
        if (RegQueryValueExA(key, SHOW_IE4_VALUE, NULL, &old_type, NULL, &old_size) == ERROR_SUCCESS)
        {
            old_exists = TRUE;
            if (old_size)
            {
                old_data = malloc(old_size);
                ok(old_data != NULL, "failed to save existing ShowIE4 value\n");
                if (old_data)
                {
                    DWORD size = old_size;
                    if (RegQueryValueExA(key, SHOW_IE4_VALUE, NULL, &old_type,
                                         old_data, &size) != ERROR_SUCCESS)
                    {
                        free(old_data);
                        old_data = NULL;
                        old_exists = FALSE;
                    }
                }
            }
        }
        RegSetValueExA(key, SHOW_IE4_VALUE, 0, REG_DWORD, (const BYTE *)&zero, sizeof(zero));

        VariantInit(&result);
        memset(&params, 0, sizeof(params));
        hr = IDispatch_Invoke(dispatch, dispid, &IID_NULL, LOCALE_SYSTEM_DEFAULT,
                              DISPATCH_PROPERTYGET, &params, &result, NULL, NULL);
        ok(hr == S_OK, "property get returned %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            if (V_VT(&result) == VT_BOOL)
                ok(V_BOOL(&result) == VARIANT_FALSE, "unexpected ShowIE4State %d\n", V_BOOL(&result));
            else if (V_VT(&result) == VT_I4)
                ok(V_I4(&result) == 0, "unexpected ShowIE4State %ld\n", V_I4(&result));
            else
                ok(0, "unexpected property variant type %u\n", V_VT(&result));
        }
        VariantClear(&result);

        VariantInit(&arg);
        V_VT(&arg) = VT_BOOL;
        V_BOOL(&arg) = VARIANT_TRUE;
        params.rgvarg = &arg;
        params.rgdispidNamedArgs = &putid;
        params.cArgs = 1;
        params.cNamedArgs = 1;

        hr = IDispatch_Invoke(dispatch, dispid, &IID_NULL, LOCALE_SYSTEM_DEFAULT,
                              DISPATCH_PROPERTYPUT, &params, NULL, NULL, NULL);
        ok(hr == S_OK, "property put returned %#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            DWORD value = 0, type = 0, size = sizeof(value);
            status = RegQueryValueExA(key, SHOW_IE4_VALUE, NULL, &type, (BYTE *)&value, &size);
            ok(status == ERROR_SUCCESS, "ShowIE4 readback returned %ld\n", status);
            ok(type == REG_DWORD && value == 1, "unexpected ShowIE4 registry value %lu type %lu\n",
               value, type);
        }

        restore_show_ie4(key, old_exists, old_type, old_data, old_size);
        free(old_data);
        RegCloseKey(key);
        if (key_created) RegDeleteKeyA(HKEY_CURRENT_USER, EXPLORER_TIPS_KEY);
    }

    IDispatch_Release(dispatch);
    ok(pDllCanUnloadNow() == S_OK, "module remained locked after object release\n");
}

START_TEST(ie4tour)
{
    HMODULE module;

    module = LoadLibraryA("ie4tour.dll");
    ok(module != NULL, "failed to load ie4tour.dll: %lu\n", GetLastError());
    if (!module) return;

    test_exports(module);
    if (pDllGetClassObject && pDllCanUnloadNow)
        test_runonce_object();

    FreeLibrary(module);
}
