/* IE PNG image filter (PNGFILT.DLL), first-stage COM implementation.
 * Copyright 2026 the Water contributors; LGPL-2.1-or-later.
 * Deliberately unregistered until pixels can reach IImageDecodeEventSink.
 */
#include <stdarg.h>
#include <string.h>
#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "objbase.h"
#include "objidl.h"
#include "wine/debug.h"
WINE_DEFAULT_DEBUG_CHANNEL(pngfilt);

/* Original FE registration still needs independent binary verification. */
static const GUID png_class = {0xa3ccedf7,0x2de2,0x11d0,{0x86,0xf4,0,0xa0,0xc9,0x13,0xf7,0x50}};
static const GUID png_filter_iid = {0xa3ccedf3,0x2de2,0x11d0,{0x86,0xf4,0,0xa0,0xc9,0x13,0xf7,0x50}};
typedef struct filter filter;
typedef struct filter_vtbl {
    HRESULT (WINAPI *QueryInterface)(filter *, REFIID, void **);
    ULONG (WINAPI *AddRef)(filter *);
    ULONG (WINAPI *Release)(filter *);
    HRESULT (WINAPI *Initialize)(filter *, IUnknown *);
    HRESULT (WINAPI *Process)(filter *, IStream *);
    HRESULT (WINAPI *Terminate)(filter *, HRESULT);
} filter_vtbl;
struct filter { const filter_vtbl *lpVtbl; };
struct filter_object { filter iface; LONG refs; IUnknown *sink; };
struct filter_factory { IClassFactory IClassFactory_iface; LONG refs; };
static LONG object_count, factory_count, lock_count;

static ULONG be32(const BYTE *p)
{
    return ((ULONG)p[0] << 24) | ((ULONG)p[1] << 16) | ((ULONG)p[2] << 8) | p[3];
}
static ULONG crc32_png(const BYTE *p, size_t size)
{
    ULONG crc = 0xffffffff;
    unsigned j;
    while (size--)
    {
        crc ^= *p++;
        for (j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320 : 0);
    }
    return crc ^ 0xffffffff;
}
static BOOL valid_ihdr(const BYTE *h)
{
    static const BYTE signature[8] = {137,80,78,71,13,10,26,10};
    ULONG width, height, depth, color;
    if (memcmp(h, signature, 8) || be32(h + 8) != 13 ||
        memcmp(h + 12, "IHDR", 4) || crc32_png(h + 12, 17) != be32(h + 29))
        return FALSE;
    width = be32(h + 16);
    height = be32(h + 20);
    depth = h[24];
    color = h[25];
    if (!width || !height || width > 32768 || height > 32768 ||
        (ULONGLONG)width * height > 100000000 || h[26] || h[27] || h[28] > 1)
        return FALSE;
    switch (color)
    {
    case 0: return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
    case 2: case 4: case 6: return depth == 8 || depth == 16;
    case 3: return depth == 1 || depth == 2 || depth == 4 || depth == 8;
    default: return FALSE;
    }
}
static struct filter_object *object_from_iface(filter *i)
{
    return CONTAINING_RECORD(i, struct filter_object, iface);
}
static HRESULT WINAPI image_QueryInterface(filter *i, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &png_filter_iid))
    {
        *out = i;
        i->lpVtbl->AddRef(i);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI image_AddRef(filter *i)
{
    return InterlockedIncrement(&object_from_iface(i)->refs);
}
static ULONG WINAPI image_Release(filter *i)
{
    struct filter_object *o = object_from_iface(i);
    ULONG refs = InterlockedDecrement(&o->refs);
    if (!refs)
    {
        if (o->sink) IUnknown_Release(o->sink);
        HeapFree(GetProcessHeap(), 0, o);
        InterlockedDecrement(&object_count);
    }
    return refs;
}
static HRESULT WINAPI image_Initialize(filter *i, IUnknown *sink)
{
    struct filter_object *o = object_from_iface(i);
    if (!sink) return E_POINTER;
    if (o->sink) return E_UNEXPECTED;
    IUnknown_AddRef(sink);
    o->sink = sink;
    return S_OK;
}
static HRESULT WINAPI image_Process(filter *i, IStream *stream)
{
    struct filter_object *o = object_from_iface(i);
    BYTE data[33];
    ULONG count = 0;
    HRESULT hr;
    if (!stream) return E_POINTER;
    if (!o->sink) return E_UNEXPECTED;
    /* IE provides a sequential-only IStream. Never seek the caller's input. */
    hr = IStream_Read(stream, data, sizeof(data), &count);
    if (FAILED(hr)) return hr;
    if (count != sizeof(data) || !valid_ihdr(data))
        return HRESULT_FROM_WIN32(ERROR_BAD_FORMAT);
    /* A valid header is not a decoded PNG or a completed image. */
    FIXME("PNG IHDR valid; no pixel delivery to IE event sink yet.\n");
    return E_NOTIMPL;
}
static HRESULT WINAPI image_Terminate(filter *i, HRESULT reason)
{
    struct filter_object *o = object_from_iface(i);
    (void)reason;
    if (o->sink) IUnknown_Release(o->sink);
    o->sink = NULL;
    return S_OK;
}
static const filter_vtbl image_vtbl = { image_QueryInterface, image_AddRef,
    image_Release, image_Initialize, image_Process, image_Terminate };

static struct filter_factory *factory_from_iface(IClassFactory *i)
{
    return CONTAINING_RECORD(i, struct filter_factory, IClassFactory_iface);
}
static HRESULT WINAPI factory_QueryInterface(IClassFactory *i, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = NULL;
    if (IsEqualIID(iid, &IID_IUnknown) || IsEqualIID(iid, &IID_IClassFactory))
    {
        *out = i;
        IClassFactory_AddRef(i);
        return S_OK;
    }
    return E_NOINTERFACE;
}
static ULONG WINAPI factory_AddRef(IClassFactory *i)
{
    return InterlockedIncrement(&factory_from_iface(i)->refs);
}
static ULONG WINAPI factory_Release(IClassFactory *i)
{
    struct filter_factory *o = factory_from_iface(i);
    ULONG refs = InterlockedDecrement(&o->refs);
    if (!refs)
    {
        HeapFree(GetProcessHeap(), 0, o);
        InterlockedDecrement(&factory_count);
    }
    return refs;
}
static HRESULT WINAPI factory_CreateInstance(IClassFactory *i, IUnknown *outer,
                                              REFIID iid, void **out)
{
    struct filter_object *obj;
    HRESULT hr;
    (void)i;
    if (!out) return E_POINTER;
    *out = NULL;
    if (outer) return CLASS_E_NOAGGREGATION;
    if (!(obj = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*obj))))
        return E_OUTOFMEMORY;
    obj->iface.lpVtbl = &image_vtbl;
    obj->refs = 1;
    InterlockedIncrement(&object_count);
    hr = image_QueryInterface(&obj->iface, iid, out);
    image_Release(&obj->iface);
    return hr;
}
static HRESULT WINAPI factory_LockServer(IClassFactory *i, BOOL lock)
{
    LONG old;
    (void)i;
    if (lock) { InterlockedIncrement(&lock_count); return S_OK; }
    do {
        old = InterlockedCompareExchange(&lock_count, 0, 0);
        if (!old) return E_UNEXPECTED;
    } while (InterlockedCompareExchange(&lock_count, old - 1, old) != old);
    return S_OK;
}
static const IClassFactoryVtbl factory_vtbl = {
    factory_QueryInterface, factory_AddRef, factory_Release,
    factory_CreateInstance, factory_LockServer
};
HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **out)
{
    struct filter_factory *factory;
    HRESULT hr;
    if (!out) return E_POINTER;
    *out = NULL;
    if (!IsEqualGUID(clsid, &png_class)) return CLASS_E_CLASSNOTAVAILABLE;
    if (!(factory = HeapAlloc(GetProcessHeap(), 0, sizeof(*factory))))
        return E_OUTOFMEMORY;
    factory->IClassFactory_iface.lpVtbl = &factory_vtbl;
    factory->refs = 1;
    InterlockedIncrement(&factory_count);
    hr = factory_QueryInterface(&factory->IClassFactory_iface, iid, out);
    factory_Release(&factory->IClassFactory_iface);
    return hr;
}
HRESULT WINAPI DllCanUnloadNow(void)
{
    return !object_count && !factory_count && !lock_count ? S_OK : S_FALSE;
}
HRESULT WINAPI DllRegisterServer(void)
{
    /* Never replace functional native PNG viewing with this partial filter. */
    return SELFREG_E_CLASS;
}
HRESULT WINAPI DllUnregisterServer(void) { return S_OK; }
