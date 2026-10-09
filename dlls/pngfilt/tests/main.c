/* PNGFILT.DLL filter contract/PNG header checks. LGPL-2.1-or-later. */
#include <stdarg.h>
#include <string.h>
#define COBJMACROS
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "objbase.h"
#include "objidl.h"
#include "wine/test.h"
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
static const GUID classid = {0xa3ccedf7,0x2de2,0x11d0,{0x86,0xf4,0,0xa0,0xc9,0x13,0xf7,0x50}};
static const GUID filterid = {0xa3ccedf3,0x2de2,0x11d0,{0x86,0xf4,0,0xa0,0xc9,0x13,0xf7,0x50}};
static const GUID unknown = {0x12345678,0,0,{0,0,0,0,0,0,1,0}};
static const BYTE png[33] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,
                              0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137};
static IStream *input(const BYTE *bytes, ULONG size)
{
    IStream *stream;
    LARGE_INTEGER start;
    ULONG written;
    if (FAILED(CreateStreamOnHGlobal(NULL, TRUE, &stream))) return NULL;
    if (FAILED(IStream_Write(stream, bytes, size, &written)) || written != size)
    {
        IStream_Release(stream); return NULL;
    }
    start.QuadPart = 0;
    IStream_Seek(stream, start, STREAM_SEEK_SET, NULL);
    return stream;
}
START_TEST(main)
{
    HMODULE lib = LoadLibraryA("pngfilt.dll");
    HRESULT (WINAPI *get)(REFCLSID, REFIID, void **);
    HRESULT (WINAPI *can_unload)(void);
    HRESULT (WINAPI *register_server)(void);
    IClassFactory *factory = NULL;
    filter *obj = NULL;
    IStream *stream;
    HRESULT hr;
    BYTE corrupt[sizeof(png)];
    void *out = (void *)0xdeadbeef;
    if (!lib) { win_skip("pngfilt.dll unavailable\n"); return; }
    get = (void *)GetProcAddress(lib, "DllGetClassObject");
    can_unload = (void *)GetProcAddress(lib, "DllCanUnloadNow");
    register_server = (void *)GetProcAddress(lib, "DllRegisterServer");
    ok(get && can_unload && register_server, "required exports missing\n");
    if (!get || !can_unload || !register_server) goto done;
    ok(can_unload() == S_OK, "unexpected live object\n");
    hr = get(&unknown, &IID_IClassFactory, &out);
    ok(hr == CLASS_E_CLASSNOTAVAILABLE && !out, "unexpected class %#lx %p\n",hr,out);
    hr = get(&classid, &IID_IClassFactory, (void **)&factory);
    ok(hr == S_OK && factory, "factory %#lx\n", hr);
    if (!factory) goto done;
    hr = IClassFactory_CreateInstance(factory, NULL, &filterid, (void **)&obj);
    ok(hr == S_OK && obj, "image filter creation %#lx\n", hr);
    if (!obj) goto release;
    ok(can_unload() == S_FALSE, "filter lifetime not counted\n");
    hr = obj->lpVtbl->Initialize(obj, NULL);
    ok(hr == E_POINTER, "null sink %#lx\n", hr);
    hr = obj->lpVtbl->Initialize(obj, (IUnknown *)factory);
    ok(hr == S_OK, "sink initialization %#lx\n", hr);
    stream = input(png, sizeof(png));
    if (stream)
    {
        hr = obj->lpVtbl->Process(obj, stream);
        ok(hr == E_NOTIMPL, "valid PNG header falsely claimed decoded %#lx\n", hr);
        IStream_Release(stream);
    }
    memcpy(corrupt, png, sizeof(png)); corrupt[0] ^= 1;
    stream = input(corrupt, sizeof(corrupt));
    if (stream)
    {
        hr = obj->lpVtbl->Process(obj, stream);
        ok(hr == HRESULT_FROM_WIN32(ERROR_BAD_FORMAT), "bad signature %#lx\n", hr);
        IStream_Release(stream);
    }
    memcpy(corrupt, png, sizeof(png)); corrupt[29] ^= 1;
    stream = input(corrupt, sizeof(corrupt));
    if (stream)
    {
        hr = obj->lpVtbl->Process(obj, stream);
        ok(hr == HRESULT_FROM_WIN32(ERROR_BAD_FORMAT), "bad IHDR CRC %#lx\n", hr);
        IStream_Release(stream);
    }
    obj->lpVtbl->Terminate(obj, E_NOTIMPL);
    obj->lpVtbl->Release(obj);
release:
    IClassFactory_Release(factory);
    ok(can_unload() == S_OK, "object or factory leaked\n");
    ok(register_server() == SELFREG_E_CLASS, "partial filter was registered\n");
done:
    FreeLibrary(lib);
}
