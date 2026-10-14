/*
 * Water COM ABI contract test for i386, x64 and ordinary ARM64 Windows.
 * Requires WIDL-generated COM headers from the configured Water build.
 * No host-native COM assumptions and no COM object registration.
 */
#include <stddef.h>
#include "objbase.h"
#include "oleauto.h"

#if defined(__cplusplus)
# define ABI_ASSERT(e, text) static_assert(e, text)
#else
# define ABI_ASSERT(e, text) _Static_assert(e, text)
#endif

ABI_ASSERT(sizeof(GUID) == 16, "GUID is a 16-byte COM identifier");
ABI_ASSERT(sizeof(VARIANT_BOOL) == 2, "Automation BOOL is 16-bit");
ABI_ASSERT(sizeof(SAFEARRAYBOUND) == 8, "SAFEARRAY bound uses 32-bit fields");
ABI_ASSERT(sizeof(IUnknown) == sizeof(void *), "IUnknown pointer ABI");
ABI_ASSERT(sizeof(IDispatch) == sizeof(void *), "IDispatch pointer ABI");

#if !defined(__cplusplus) || defined(CINTERFACE)
/* C and CINTERFACE have a genuine table of three and seven pointers. */
ABI_ASSERT(sizeof(IUnknownVtbl) == 3 * sizeof(void *), "IUnknown slot count");
ABI_ASSERT(offsetof(IUnknownVtbl, QueryInterface) == 0 * sizeof(void *), "QueryInterface slot");
ABI_ASSERT(offsetof(IUnknownVtbl, AddRef) == 1 * sizeof(void *), "AddRef slot");
ABI_ASSERT(offsetof(IUnknownVtbl, Release) == 2 * sizeof(void *), "Release slot");
ABI_ASSERT(sizeof(IDispatchVtbl) == 7 * sizeof(void *), "IDispatch slot count");
ABI_ASSERT(offsetof(IDispatchVtbl, GetTypeInfoCount) == 3 * sizeof(void *), "IDispatch slot 3");
ABI_ASSERT(offsetof(IDispatchVtbl, GetTypeInfo) == 4 * sizeof(void *), "IDispatch slot 4");
ABI_ASSERT(offsetof(IDispatchVtbl, GetIDsOfNames) == 5 * sizeof(void *), "IDispatch slot 5");
ABI_ASSERT(offsetof(IDispatchVtbl, Invoke) == 6 * sizeof(void *), "IDispatch slot 6");
#endif

#if defined(_WIN64)
ABI_ASSERT(sizeof(void *) == 8, "64-bit Windows pointer");
ABI_ASSERT(sizeof(VARIANT) == 24, "64-bit VARIANT layout");
/* Use the SDK accessor so both named and anonymous union modes compile. */
static VARTYPE water_variant_type(VARIANT *value)
{
    return V_VT(value);
}
ABI_ASSERT(sizeof(SAFEARRAY) == 32, "64-bit SAFEARRAY descriptor");
ABI_ASSERT(offsetof(SAFEARRAY, pvData) == 16, "64-bit SAFEARRAY data pointer");
ABI_ASSERT(offsetof(SAFEARRAY, rgsabound) == 24, "64-bit SAFEARRAY bounds");
ABI_ASSERT(sizeof(DISPPARAMS) == 24, "64-bit DISPPARAMS");
#else
ABI_ASSERT(sizeof(void *) == 4, "32-bit Windows pointer");
ABI_ASSERT(sizeof(VARIANT) == 16, "32-bit VARIANT layout");
ABI_ASSERT(sizeof(SAFEARRAY) == 24, "32-bit SAFEARRAY descriptor");
ABI_ASSERT(offsetof(SAFEARRAY, pvData) == 12, "32-bit SAFEARRAY data pointer");
ABI_ASSERT(offsetof(SAFEARRAY, rgsabound) == 16, "32-bit SAFEARRAY bounds");
ABI_ASSERT(sizeof(DISPPARAMS) == 16, "32-bit DISPPARAMS");
#endif

/* This SDK macro was missing from objbase.h. A root interface has no
 * C++ base, while the C formulation must have a real three-slot table.
 * Derived interfaces must preserve base slots and C++ inheritance. */
#define INTERFACE IWaterRoot
DECLARE_INTERFACE_IID(IWaterRoot, "b2b15f23-7cc1-4214-9257-50b3c0627071")
{
    BEGIN_INTERFACE
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    END_INTERFACE
};
#undef INTERFACE
#define INTERFACE IWaterChild
DECLARE_INTERFACE_IID_(IWaterChild, IUnknown, "5752be63-ff8b-496c-9b4c-89e48abfd420")
{
    BEGIN_INTERFACE
#if !defined(__cplusplus) || defined(CINTERFACE)
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
#endif
    STDMETHOD(Read)(THIS_ int *result) PURE;
    END_INTERFACE
};
#undef INTERFACE

ABI_ASSERT(sizeof(IWaterRoot) == sizeof(void *), "root COM interface pointer");
ABI_ASSERT(sizeof(IWaterChild) == sizeof(void *), "derived COM interface pointer");

#if !defined(__cplusplus) || defined(CINTERFACE)
ABI_ASSERT(sizeof(IWaterRootVtbl) == 3 * sizeof(void *), "root C slots");
ABI_ASSERT(sizeof(IWaterChildVtbl) == 4 * sizeof(void *), "derived C slots");
ABI_ASSERT(offsetof(IWaterChildVtbl, Read) == 3 * sizeof(void *), "derived method slot");
#endif

#ifdef __cplusplus
#if !defined(CINTERFACE)
/* The MSVC ABI must retain GUID metadata for a root COM interface. */
static const GUID *root_iid()
{
    return &__uuidof(IWaterRoot);
}
static const GUID *derived_iid()
{
    return &__uuidof(IWaterChild);
}
static HRESULT create_child(IWaterChild **result)
{
    return CoCreateInstance(CLSID_NULL, nullptr, CLSCTX_INPROC_SERVER,
                            IID_PPV_ARGS(result));
}
#endif
#endif
