/*
 * Windows 9x MSDOS.SYS configuration
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "kernel16_private.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dos);

/*
 * Windows 95 and later 9x releases use a text MSDOS.SYS in the root of the
 * boot drive. Keep this separate from the pre-Windows-95 binary MSDOS.SYS.
 */
static BOOL get_msdos_sys_path( char path[MAX_PATH] )
{
    char drive[4] = "C:\\";
    char windows[MAX_PATH];
    DWORD len;

    len = GetEnvironmentVariableA( "SystemDrive", drive, sizeof(drive) );
    if (len != 2 || drive[1] != ':')
    {
        len = GetWindowsDirectoryA( windows, sizeof(windows) );
        if (len >= 2 && len < sizeof(windows) && windows[1] == ':')
        {
            drive[0] = windows[0];
            drive[1] = ':';
            drive[2] = '\\';
            drive[3] = 0;
        }
        else
            strcpy( drive, "C:\\" );
    }
    else
    {
        drive[2] = '\\';
        drive[3] = 0;
    }

    if (snprintf( path, MAX_PATH, "%sMSDOS.SYS", drive ) >= MAX_PATH)
        return FALSE;

    return GetFileAttributesA( path ) != INVALID_FILE_ATTRIBUTES;
}

static void get_msdos_path_value( const char *filename, const char *name,
                                  const char *default_value, char *buffer, DWORD size )
{
    GetPrivateProfileStringA( "Paths", name, default_value, buffer, size, filename );
}

/***********************************************************************
 *           MSDOS_InitConfig
 *
 * Initialize the Win9x boot-directory environment from MSDOS.SYS.
 *
 * Windows 9x stores WinDir and WinBootDir in the [Paths] section and exposes
 * the resulting paths as the lowercase windir and winbootdir environment
 * variables. If MSDOS.SYS is absent, use the active Windows directory for
 * both values rather than inventing a separate boot path.
 */
void MSDOS_InitConfig(void)
{
    RTL_OSVERSIONINFOEXW info;
    char filename[MAX_PATH], windows[MAX_PATH];
    char windir[MAX_PATH], winbootdir[MAX_PATH], host_drive[16];
    DWORD len;
    BOOL have_file;

    info.dwOSVersionInfoSize = sizeof(info);
    if (RtlGetVersion( &info ) || info.dwPlatformId != VER_PLATFORM_WIN32_WINDOWS)
        return;

    len = GetWindowsDirectoryA( windows, sizeof(windows) );
    if (!len || len >= sizeof(windows))
        return;

    strcpy( windir, windows );
    strcpy( winbootdir, windows );

    have_file = get_msdos_sys_path( filename );
    if (have_file)
    {
        get_msdos_path_value( filename, "WinDir", windows, windir, sizeof(windir) );
        get_msdos_path_value( filename, "WinBootDir", windir, winbootdir, sizeof(winbootdir) );
        get_msdos_path_value( filename, "HostWinBootDrv", "", host_drive, sizeof(host_drive) );

        TRACE( "%s: WinDir=%s WinBootDir=%s HostWinBootDrv=%s\n",
               debugstr_a(filename), debugstr_a(windir), debugstr_a(winbootdir),
               debugstr_a(host_drive) );
    }
    else
        TRACE( "no boot-drive MSDOS.SYS, using Windows directory %s\n", debugstr_a(windows) );

    SetEnvironmentVariableA( "windir", windir );
    SetEnvironmentVariableA( "winbootdir", winbootdir );
}
