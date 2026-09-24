/*
 * OLE 1 server library tests
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include "windef.h"
#include "winbase.h"

#include "wine/test.h"

typedef LONG_PTR LHSERVER;
typedef LONG_PTR LHSERVERDOC;
typedef int OLESTATUS;

enum
{
    OLE_OK,
    OLE_WAIT_FOR_RELEASE,
    OLE_BUSY,
    OLE_ERROR_PROTECT_ONLY,
    OLE_ERROR_MEMORY,
    OLE_ERROR_STREAM,
    OLE_ERROR_STATIC,
    OLE_ERROR_BLANK,
    OLE_ERROR_DRAW,
    OLE_ERROR_METAFILE,
    OLE_ERROR_ABORT,
    OLE_ERROR_CLIPBOARD,
    OLE_ERROR_FORMAT,
    OLE_ERROR_OBJECT,
    OLE_ERROR_OPTION,
    OLE_ERROR_PROTOCOL,
    OLE_ERROR_ADDRESS,
    OLE_ERROR_NOT_EQUAL,
    OLE_ERROR_HANDLE
};

typedef enum
{
    OLE_SERVER_MULTI,
    OLE_SERVER_SINGLE
} OLE_SERVER_USE;

struct ole_server
{
    const void *lpvtbl;
};

struct ole_server_doc
{
    const void *lpvtbl;
};

static OLESTATUS (WINAPI *pOleRegisterServer)(LPCSTR, struct ole_server *, LHSERVER *,
        HINSTANCE, OLE_SERVER_USE);
static OLESTATUS (WINAPI *pOleRevokeServer)(LHSERVER);
static OLESTATUS (WINAPI *pOleBlockServer)(LHSERVER);
static OLESTATUS (WINAPI *pOleUnblockServer)(LHSERVER, BOOL *);
static OLESTATUS (WINAPI *pOleRegisterServerDoc)(LHSERVER, LPCSTR, struct ole_server_doc *,
        LHSERVERDOC *);
static OLESTATUS (WINAPI *pOleRevokeServerDoc)(LHSERVERDOC);
static OLESTATUS (WINAPI *pOleRenameServerDoc)(LHSERVERDOC, LPCSTR);
static OLESTATUS (WINAPI *pOleRevertServerDoc)(LHSERVERDOC);
static OLESTATUS (WINAPI *pOleSavedServerDoc)(LHSERVERDOC);

static void test_registration(void)
{
    static const void *dummy_vtbl = (void *)1;
    struct ole_server server = { &dummy_vtbl };
    struct ole_server_doc document = { &dummy_vtbl };
    LHSERVER server_handle = 0;
    LHSERVERDOC doc_handle = 0;
    BOOL blocked = TRUE;
    OLESTATUS status;

    status = pOleRegisterServer("WineTest.Server", &server, NULL, NULL, OLE_SERVER_MULTI);
    ok(status == OLE_ERROR_ADDRESS, "got status %d.\n", status);

    status = pOleRegisterServer("WineTest.Server", &server, &server_handle, NULL, OLE_SERVER_MULTI);
    ok(status == OLE_OK, "got status %d.\n", status);
    ok(!!server_handle, "server handle is zero.\n");

    status = pOleBlockServer(server_handle);
    ok(status == OLE_OK, "got status %d.\n", status);

    status = pOleUnblockServer(server_handle, &blocked);
    ok(status == OLE_OK, "got status %d.\n", status);
    ok(!blocked, "server remained blocked.\n");

    status = pOleBlockServer(server_handle + 0x10000);
    ok(status == OLE_ERROR_HANDLE, "got status %d.\n", status);

    status = pOleRegisterServerDoc(server_handle + 0x10000, "bad", &document, &doc_handle);
    ok(status == OLE_ERROR_HANDLE, "got status %d.\n", status);
    ok(!doc_handle, "unexpected document handle %Ix.\n", (ULONG_PTR)doc_handle);

    status = pOleRegisterServerDoc(server_handle, "Document", &document, &doc_handle);
    ok(status == OLE_OK, "got status %d.\n", status);
    ok(!!doc_handle, "document handle is zero.\n");

    status = pOleRenameServerDoc(doc_handle, "Renamed");
    ok(status == OLE_OK, "got status %d.\n", status);

    status = pOleRenameServerDoc(doc_handle, NULL);
    ok(status == OLE_ERROR_ADDRESS, "got status %d.\n", status);

    status = pOleSavedServerDoc(doc_handle);
    ok(status == OLE_OK, "got status %d.\n", status);

    status = pOleRevertServerDoc(doc_handle);
    ok(status == OLE_OK, "got status %d.\n", status);

    status = pOleRevokeServerDoc(doc_handle);
    ok(status == OLE_OK, "got status %d.\n", status);
    status = pOleRevokeServerDoc(doc_handle);
    ok(status == OLE_ERROR_HANDLE, "got status %d.\n", status);

    status = pOleRevokeServer(server_handle);
    ok(status == OLE_OK, "got status %d.\n", status);
    status = pOleRevokeServer(server_handle);
    ok(status == OLE_ERROR_HANDLE, "got status %d.\n", status);
}

static void test_legacy_exports(HMODULE module)
{
    static const char *const names[] =
    {
        "SrvrWndProc", "DocWndProc", "ItemWndProc", "SendDataMsg", "FindItemWnd",
        "ItemCallBack", "TerminateClients", "TerminateDocClients", "DeleteClientInfo",
        "SendRenameMsg", "EnumForTerminate"
    };
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(names); ++i)
        ok(!!GetProcAddress(module, names[i]), "missing export %s.\n", names[i]);
}

START_TEST(server)
{
    HMODULE module = GetModuleHandleA("olesvr32.dll");

    ok(!!module, "olesvr32.dll is not loaded.\n");
    if (!module) return;

    pOleRegisterServer = (void *)GetProcAddress(module, "OleRegisterServer");
    pOleRevokeServer = (void *)GetProcAddress(module, "OleRevokeServer");
    pOleBlockServer = (void *)GetProcAddress(module, "OleBlockServer");
    pOleUnblockServer = (void *)GetProcAddress(module, "OleUnblockServer");
    pOleRegisterServerDoc = (void *)GetProcAddress(module, "OleRegisterServerDoc");
    pOleRevokeServerDoc = (void *)GetProcAddress(module, "OleRevokeServerDoc");
    pOleRenameServerDoc = (void *)GetProcAddress(module, "OleRenameServerDoc");
    pOleRevertServerDoc = (void *)GetProcAddress(module, "OleRevertServerDoc");
    pOleSavedServerDoc = (void *)GetProcAddress(module, "OleSavedServerDoc");

    ok(!!pOleRegisterServer, "OleRegisterServer is missing.\n");
    ok(!!pOleRevokeServer, "OleRevokeServer is missing.\n");
    ok(!!pOleBlockServer, "OleBlockServer is missing.\n");
    ok(!!pOleUnblockServer, "OleUnblockServer is missing.\n");
    ok(!!pOleRegisterServerDoc, "OleRegisterServerDoc is missing.\n");
    ok(!!pOleRevokeServerDoc, "OleRevokeServerDoc is missing.\n");
    ok(!!pOleRenameServerDoc, "OleRenameServerDoc is missing.\n");
    ok(!!pOleRevertServerDoc, "OleRevertServerDoc is missing.\n");
    ok(!!pOleSavedServerDoc, "OleSavedServerDoc is missing.\n");

    if (pOleRegisterServer && pOleRevokeServer && pOleBlockServer && pOleUnblockServer &&
            pOleRegisterServerDoc && pOleRevokeServerDoc && pOleRenameServerDoc &&
            pOleRevertServerDoc && pOleSavedServerDoc)
        test_registration();

    test_legacy_exports(module);
}
