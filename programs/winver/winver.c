/*
 * Windows Version Program
 *
 * Copyright 1997 by Marcel Baur (mbaur@g26.ethz.ch)
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

#include <stdio.h>

#include "windows.h"
#include "commctrl.h"
#include "shellapi.h"

int PASCAL WinMain (HINSTANCE inst, HINSTANCE prev, LPSTR cmdline, int show)
{
    char name[128] = "Wine";
    char os_version[128] = "";
    const char * (CDECL *wine_get_version)(void);
    OSVERSIONINFOEXA version = {0};
    HMODULE ntdll;
    DWORD build;

    InitCommonControls();

    ntdll = GetModuleHandleA("ntdll.dll");
    wine_get_version = ntdll ? (void *)GetProcAddress( ntdll, "wine_get_version" ) : NULL;
    if (wine_get_version) snprintf( name, sizeof(name), "Wine %s", wine_get_version() );

    version.dwOSVersionInfoSize = sizeof(version);
    if (GetVersionExA( (OSVERSIONINFOA *)&version ))
    {
        build = version.dwBuildNumber;
        if (version.dwPlatformId != VER_PLATFORM_WIN32_NT) build = LOWORD(build);
        snprintf( os_version, sizeof(os_version), "Windows %lu.%lu (Build %lu)",
                  version.dwMajorVersion, version.dwMinorVersion, build );
    }

    return !ShellAboutA( NULL, name, os_version[0] ? os_version : NULL, 0 );
}
