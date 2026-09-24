/*
 *	OLESVR library
 *
 *	Copyright 1995	Martin von Loewis
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

/*	At the moment, these are only empty stubs.
 */

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ole);

typedef enum
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
    OLE_ERROR_HANDLE,
    OLE_ERROR_GENERIC,
    OLE_ERROR_CLASS,
    OLE_ERROR_SYNTAX,
    OLE_ERROR_DATATYPE,
    OLE_ERROR_PALETTE,
    OLE_ERROR_NOT_LINK,
    OLE_ERROR_NOT_EMPTY,
    OLE_ERROR_SIZE,
    OLE_ERROR_DRIVE,
    OLE_ERROR_NETWORK,
    OLE_ERROR_NAME,
    OLE_ERROR_TEMPLATE,
    OLE_ERROR_NEW,
    OLE_ERROR_EDIT,
    OLE_ERROR_OPEN,
    OLE_ERROR_NOT_OPEN,
    OLE_ERROR_LAUNCH,
    OLE_ERROR_COMM,
    OLE_ERROR_TERMINATE,
    OLE_ERROR_COMMAND,
    OLE_ERROR_SHOW,
    OLE_ERROR_DOVERB,
    OLE_ERROR_ADVISE_NATIVE,
    OLE_ERROR_ADVISE_PICT,
    OLE_ERROR_ADVISE_RENAME,
    OLE_ERROR_POKE_NATIVE,
    OLE_ERROR_REQUEST_NATIVE,
    OLE_ERROR_REQUEST_PICT,
    OLE_ERROR_SERVER_BLOCKED,
    OLE_ERROR_REGISTRATION,
    OLE_ERROR_ALREADY_REGISTERED,
    OLE_ERROR_TASK,
    OLE_ERROR_OUTOFDATE,
    OLE_ERROR_CANT_UPDATE_CLIENT,
    OLE_ERROR_UPDATE,
    OLE_ERROR_SETDATA_FORMAT,
    OLE_ERROR_STATIC_FROM_OTHER_OS,
    OLE_WARN_DELETE_DATA = 1000
} OLESTATUS;

typedef enum {
    OLE_SERVER_MULTI,
    OLE_SERVER_SINGLE
} OLE_SERVER_USE;

typedef LONG LHSERVER;
typedef LONG LHSERVERDOC;
typedef LPCSTR LPCOLESTR16;

typedef struct _OLESERVERDOC *LPOLESERVERDOC;

struct _OLESERVERDOCVTBL;
typedef struct _OLESERVERDOC
{
    const struct _OLESERVERDOCVTBL *lpvtbl;
    /* server provided state info */
} OLESERVERDOC;

typedef struct _OLESERVER *LPOLESERVER;
typedef struct _OLESERVERVTBL
{
    OLESTATUS (CALLBACK *Open)(LPOLESERVER,LHSERVERDOC,LPCOLESTR16,LPOLESERVERDOC *);
    OLESTATUS (CALLBACK *Create)(LPOLESERVER,LHSERVERDOC,LPCOLESTR16,LPCOLESTR16,LPOLESERVERDOC*);
    OLESTATUS (CALLBACK *CreateFromTemplate)(LPOLESERVER,LHSERVERDOC,LPCOLESTR16,LPCOLESTR16,LPCOLESTR16,LPOLESERVERDOC *);
    OLESTATUS (CALLBACK *Edit)(LPOLESERVER,LHSERVERDOC,LPCOLESTR16,LPCOLESTR16,LPOLESERVERDOC *);
    OLESTATUS (CALLBACK *Exit)(LPOLESERVER);
    OLESTATUS (CALLBACK *Release)(LPOLESERVER);
    OLESTATUS (CALLBACK *Execute)(LPOLESERVER);
} OLESERVERVTBL, *LPOLESERVERVTBL;

typedef struct _OLESERVER
{
    const OLESERVERVTBL *lpvtbl;
    /* server specific data */
} OLESERVER;

static LONG OLE_current_handle;
static SRWLOCK server_lock = SRWLOCK_INIT;

struct server_entry
{
    struct server_entry *next;
    LHSERVER handle;
    char *name;
    LPOLESERVER server;
    HINSTANCE instance;
    OLE_SERVER_USE use;
    BOOL blocked;
};

struct document_entry
{
    struct document_entry *next;
    LHSERVERDOC handle;
    LHSERVER server;
    char *name;
    LPOLESERVERDOC document;
    BOOL saved;
};

static struct server_entry *servers;
static struct document_entry *documents;

static char *heap_strdupA(const char *str)
{
    SIZE_T size;
    char *ret;

    if (!str) return NULL;

    size = strlen(str) + 1;
    if (!(ret = HeapAlloc(GetProcessHeap(), 0, size))) return NULL;
    memcpy(ret, str, size);
    return ret;
}

static LONG next_handle(void)
{
    LONG handle;

    do
        handle = InterlockedIncrement(&OLE_current_handle);
    while (!handle);

    return handle;
}

static struct server_entry *find_server(LHSERVER handle)
{
    struct server_entry *server;

    for (server = servers; server; server = server->next)
        if (server->handle == handle) return server;

    return NULL;
}

static struct document_entry *find_document(LHSERVERDOC handle)
{
    struct document_entry *document;

    for (document = documents; document; document = document->next)
        if (document->handle == handle) return document;

    return NULL;
}

/******************************************************************************
 *              OleBlockServer  [OLESVR32.4]
 */
OLESTATUS WINAPI OleBlockServer(LHSERVER hServer)
{
    struct server_entry *server;
    OLESTATUS status = OLE_OK;

    TRACE("(%ld)\n", hServer);

    AcquireSRWLockExclusive(&server_lock);
    if (!(server = find_server(hServer)))
        status = OLE_ERROR_HANDLE;
    else
        server->blocked = TRUE;
    ReleaseSRWLockExclusive(&server_lock);

    return status;
}

/******************************************************************************
 *              OleUnblockServer        [OLESVR32.5]
 */
OLESTATUS WINAPI OleUnblockServer(LHSERVER hServer, BOOL *block)
{
    struct server_entry *server;
    OLESTATUS status = OLE_OK;

    TRACE("(%ld,%p)\n", hServer, block);

    if (!block) return OLE_ERROR_ADDRESS;

    AcquireSRWLockExclusive(&server_lock);
    if (!(server = find_server(hServer)))
    {
        *block = FALSE;
        status = OLE_ERROR_HANDLE;
    }
    else
    {
        server->blocked = FALSE;
        *block = FALSE;
    }
    ReleaseSRWLockExclusive(&server_lock);

    return status;
}

/******************************************************************************
 *              OleRevokeServerDoc      [OLESVR32.7]
 */
OLESTATUS WINAPI OleRevokeServerDoc(LHSERVERDOC hServerDoc)
{
    struct document_entry **cursor, *document;

    TRACE("(%ld)\n", hServerDoc);

    AcquireSRWLockExclusive(&server_lock);
    for (cursor = &documents; (document = *cursor); cursor = &document->next)
    {
        if (document->handle != hServerDoc) continue;

        *cursor = document->next;
        ReleaseSRWLockExclusive(&server_lock);
        HeapFree(GetProcessHeap(), 0, document->name);
        HeapFree(GetProcessHeap(), 0, document);
        return OLE_OK;
    }
    ReleaseSRWLockExclusive(&server_lock);

    return OLE_ERROR_HANDLE;
}

/******************************************************************************
 *              OleRegisterServer       [OLESVR32.2]
 */
OLESTATUS WINAPI OleRegisterServer(LPCSTR svrname, LPOLESERVER olesvr, LHSERVER *hRet,
        HINSTANCE hinst, OLE_SERVER_USE use)
{
    struct server_entry *server;

    TRACE("(%s,%p,%p,%p,%d)\n", debugstr_a(svrname), olesvr, hRet, hinst, use);

    if (!svrname || !olesvr || !olesvr->lpvtbl || !hRet)
        return OLE_ERROR_ADDRESS;
    if (use != OLE_SERVER_MULTI && use != OLE_SERVER_SINGLE)
        return OLE_ERROR_OPTION;

    *hRet = 0;
    if (!(server = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*server))))
        return OLE_ERROR_MEMORY;
    if (!(server->name = heap_strdupA(svrname)))
    {
        HeapFree(GetProcessHeap(), 0, server);
        return OLE_ERROR_MEMORY;
    }

    server->handle = next_handle();
    server->server = olesvr;
    server->instance = hinst;
    server->use = use;

    AcquireSRWLockExclusive(&server_lock);
    server->next = servers;
    servers = server;
    ReleaseSRWLockExclusive(&server_lock);

    *hRet = server->handle;
    return OLE_OK;
}

/******************************************************************************
 *              OleRegisterServerDoc    [OLESVR32.6]
 */
OLESTATUS WINAPI OleRegisterServerDoc(LHSERVER hServer, LPCSTR docname,
        LPOLESERVERDOC document, LHSERVERDOC *hRet)
{
    struct document_entry *entry;

    TRACE("(%ld,%s,%p,%p)\n", hServer, debugstr_a(docname), document, hRet);

    if (!docname || !document || !document->lpvtbl || !hRet)
        return OLE_ERROR_ADDRESS;

    *hRet = 0;

    AcquireSRWLockExclusive(&server_lock);
    if (!find_server(hServer))
    {
        ReleaseSRWLockExclusive(&server_lock);
        return OLE_ERROR_HANDLE;
    }
    ReleaseSRWLockExclusive(&server_lock);

    if (!(entry = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*entry))))
        return OLE_ERROR_MEMORY;
    if (!(entry->name = heap_strdupA(docname)))
    {
        HeapFree(GetProcessHeap(), 0, entry);
        return OLE_ERROR_MEMORY;
    }

    entry->handle = next_handle();
    entry->server = hServer;
    entry->document = document;
    entry->saved = TRUE;

    AcquireSRWLockExclusive(&server_lock);
    if (!find_server(hServer))
    {
        ReleaseSRWLockExclusive(&server_lock);
        HeapFree(GetProcessHeap(), 0, entry->name);
        HeapFree(GetProcessHeap(), 0, entry);
        return OLE_ERROR_HANDLE;
    }
    entry->next = documents;
    documents = entry;
    ReleaseSRWLockExclusive(&server_lock);

    *hRet = entry->handle;
    return OLE_OK;
}

/******************************************************************************
 *              OleRenameServerDoc      [OLESVR32.8]
 */
OLESTATUS WINAPI OleRenameServerDoc(LHSERVERDOC hDoc, LPCSTR newName)
{
    struct document_entry *document;
    char *name;

    TRACE("(%ld,%s)\n", hDoc, debugstr_a(newName));

    if (!newName) return OLE_ERROR_ADDRESS;
    if (!(name = heap_strdupA(newName))) return OLE_ERROR_MEMORY;

    AcquireSRWLockExclusive(&server_lock);
    if (!(document = find_document(hDoc)))
    {
        ReleaseSRWLockExclusive(&server_lock);
        HeapFree(GetProcessHeap(), 0, name);
        return OLE_ERROR_HANDLE;
    }

    HeapFree(GetProcessHeap(), 0, document->name);
    document->name = name;
    document->saved = FALSE;
    ReleaseSRWLockExclusive(&server_lock);

    return OLE_OK;
}

/******************************************************************************
 *              OleRevertServerDoc      [OLESVR32.9]
 */
OLESTATUS WINAPI OleRevertServerDoc(LHSERVERDOC hDoc)
{
    struct document_entry *document;
    OLESTATUS status = OLE_OK;

    TRACE("(%ld)\n", hDoc);

    AcquireSRWLockExclusive(&server_lock);
    if (!(document = find_document(hDoc)))
        status = OLE_ERROR_HANDLE;
    else
        document->saved = TRUE;
    ReleaseSRWLockExclusive(&server_lock);

    return status;
}

/******************************************************************************
 *              OleSavedServerDoc       [OLESVR32.10]
 */
OLESTATUS WINAPI OleSavedServerDoc(LHSERVERDOC hDoc)
{
    struct document_entry *document;
    OLESTATUS status = OLE_OK;

    TRACE("(%ld)\n", hDoc);

    AcquireSRWLockExclusive(&server_lock);
    if (!(document = find_document(hDoc)))
        status = OLE_ERROR_HANDLE;
    else
        document->saved = TRUE;
    ReleaseSRWLockExclusive(&server_lock);

    return status;
}

/******************************************************************************
 *              OleRevokeServer [OLESVR32.3]
 */
OLESTATUS WINAPI OleRevokeServer(LHSERVER hServer)
{
    struct document_entry **doc_cursor, *document;
    struct server_entry **cursor, *server;

    TRACE("(%ld)\n", hServer);

    AcquireSRWLockExclusive(&server_lock);
    for (cursor = &servers; (server = *cursor); cursor = &server->next)
    {
        if (server->handle != hServer) continue;

        *cursor = server->next;

        doc_cursor = &documents;
        while ((document = *doc_cursor))
        {
            if (document->server != hServer)
            {
                doc_cursor = &document->next;
                continue;
            }

            *doc_cursor = document->next;
            HeapFree(GetProcessHeap(), 0, document->name);
            HeapFree(GetProcessHeap(), 0, document);
        }

        ReleaseSRWLockExclusive(&server_lock);
        HeapFree(GetProcessHeap(), 0, server->name);
        HeapFree(GetProcessHeap(), 0, server);
        return OLE_OK;
    }
    ReleaseSRWLockExclusive(&server_lock);

    return OLE_ERROR_HANDLE;
}
