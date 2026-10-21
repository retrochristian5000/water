/*
 * Windows 2000 IIS 5 service host (inetinfo.exe).
 *
 * Copyright 2026 WHP contributors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * This first stage provides the shared-process SCM entry point only.
 * IIS metabase, HTTP, FTP, SMTP and NNTP backends are not implemented.
 * Keep all services stopped until those backends exist: returning a
 * running status would mislead applications and administrators.
 * In particular, do not wire this host to httpapi.dll / http.sys.
 */

#include <stdarg.h>

#define WIN32_LEAN_AND_MEAN
#include "windef.h"
#include "winbase.h"
#include "winsvc.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(inetinfo);

static WCHAR iisadminW[] = L"IISADMIN";
static WCHAR w3svcW[]    = L"W3SVC";
static WCHAR msftpsvcW[] = L"MSFTPSVC";
static WCHAR smtpsvcW[]  = L"SMTPSVC";
static WCHAR nntpsvcW[]  = L"NNTPSVC";

static void WINAPI service_handler(DWORD control)
{
    /* No controls are accepted by a service which has not started. */
    WARN("Unexpected service control %lu.\n", control);
}

static void WINAPI service_main(DWORD argc, WCHAR **argv)
{
    SERVICE_STATUS status = {0};
    SERVICE_STATUS_HANDLE handle;
    const WCHAR *name;

    /* For a shared-process service, argv[0] contains its service name. */
    if (!argc || !argv || !argv[0])
    {
        ERR("SCM supplied no service name.\n");
        return;
    }
    name = argv[0];

    handle = RegisterServiceCtrlHandlerW(name, service_handler);
    if (!handle)
    {
        ERR("Could not register %s service handler: %lu.\n", debugstr_w(name), GetLastError());
        return;
    }

    /* Fail closed: the real IIS service implementation is not yet available. */
    status.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    status.dwCurrentState = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    status.dwWin32ExitCode = ERROR_CALL_NOT_IMPLEMENTED;
    FIXME("%s needs a Windows 2000 IIS service backend.\n", debugstr_w(name));
    /* Successful SERVICE_STOPPED can cause SCM to terminate this process. */
    if (!SetServiceStatus(handle, &status))
        ERR("Could not report %s service status: %lu.\n", debugstr_w(name), GetLastError());
}

int __cdecl wmain(int argc, WCHAR **argv)
{
    SERVICE_TABLE_ENTRYW table[] =
    {
        { iisadminW, service_main },
        { w3svcW, service_main },
        { msftpsvcW, service_main },
        { smtpsvcW, service_main },
        { nntpsvcW, service_main },
        { NULL, NULL }
    };

    (void)argc;
    (void)argv;

    if (!StartServiceCtrlDispatcherW(table))
    {
        DWORD error = GetLastError();
        ERR("IIS service dispatcher failed: %lu.\n", error);
        return error;
    }
    return 0;
}
