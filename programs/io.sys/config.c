/*
 * Windows 9x IO.SYS boot configuration support
 *
 * IO.SYS owns MSDOS.SYS parsing and the real-mode startup policy that runs
 * before WIN.COM.  This source remains callable from KRNL386 only through a
 * temporary compatibility bridge until Water boots its own IO.SYS image.
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "io_sys.h"
#include "winternl.h"
#include "kernel16_private.h"
#include "win386.h"
#include "wine/win16_profile.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(dos);

/* Boot drive reported by the Win9x IO.SYS/MSDOS.SYS startup state. */
static BYTE iosys_boot_drive;

static BYTE drive_number_from_string( const char *value )
{
    char drive;

    if (!value || !(drive = value[0])) return 0;
    if (drive >= 'a' && drive <= 'z') drive -= 'a' - 'A';
    if (drive < 'A' || drive > 'Z') return 0;
    if (value[1] && value[1] != ':') return 0;
    return drive - 'A' + 1;
}

/*
 * Windows 95 and later 9x releases use a text MSDOS.SYS in the root of the
 * boot drive. Keep this separate from the pre-Windows-95 binary MSDOS.SYS.
 */
static BOOL is_msdos_sys_file( const char *path )
{
    DWORD attrs = GetFileAttributesA( path );

    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

static BOOL try_msdos_sys_drive( char drive, char path[MAX_PATH] )
{
    if (snprintf( path, MAX_PATH, "%c:\\MSDOS.SYS", drive ) >= MAX_PATH)
        return FALSE;

    return is_msdos_sys_file( path );
}

static BOOL get_msdos_sys_path( char path[MAX_PATH] )
{
    char system_drive[4];
    char windows[MAX_PATH];
    DWORD len;

    len = GetEnvironmentVariableA( "SystemDrive", system_drive, sizeof(system_drive) );
    if (len == 2 && system_drive[1] == ':' && try_msdos_sys_drive( system_drive[0], path ))
        return TRUE;

    /* C: is the normal Win9x boot root even when Windows itself is elsewhere. */
    if ((len != 2 || system_drive[0] != 'C') && try_msdos_sys_drive( 'C', path ))
        return TRUE;

    len = GetWindowsDirectoryA( windows, sizeof(windows) );
    if (len >= 2 && len < sizeof(windows) && windows[1] == ':' &&
        (windows[0] != 'C') &&
        !(GetEnvironmentVariableA( "SystemDrive", system_drive, sizeof(system_drive) ) == 2 &&
          system_drive[1] == ':' && system_drive[0] == windows[0]) &&
        try_msdos_sys_drive( windows[0], path ))
        return TRUE;

    path[0] = 0;
    return FALSE;
}

static void get_msdos_path_value( const char *filename, const char *name,
                                  const char *default_value, char *buffer, DWORD size )
{
    GetPrivateProfileStringA( "Paths", name, default_value, buffer, size, filename );
}

static WORD iosys_get_word( const BYTE *p )
{
    return p[0] | (p[1] << 8);
}

static DWORD iosys_get_dword( const BYTE *p )
{
    return iosys_get_word( p ) | ((DWORD)iosys_get_word( p + 2 ) << 16);
}

/***********************************************************************
 *           IOSYS_GetFat1216BPB
 *
 * Read the classic FAT12/FAT16 BIOS parameter block from a DOS drive's boot
 * sector.  IO.SYS/device drivers are the historical source of this geometry;
 * DOS later exposes it through drive parameter blocks.
 *
 * The drive argument is zero based (0=A:, 1=B:, ...), matching DOS DPBs.
 * FAT32 deliberately returns FALSE here because its root-directory and FAT
 * geometry use the extended BPB rather than these classic fields.
 */
BOOL IOSYS_GetFat1216BPB(BYTE drive, struct iosys_fat_bpb *bpb)
{
    WCHAR volume[] = {'\\','\\','.','\\','A',':',0};
    BYTE sector[512];
    DWORD read;
    DWORD total_sectors, root_sectors, first_data, data_sectors, clusters;
    HANDLE file;

    if (!bpb || drive >= 26) return FALSE;

    volume[4] += drive;
    file = CreateFileW( volume, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    read = 0;
    if (!ReadFile( file, sector, sizeof(sector), &read, NULL ) || read != sizeof(sector))
    {
        CloseHandle( file );
        return FALSE;
    }
    CloseHandle( file );

    if (sector[510] != 0x55 || sector[511] != 0xaa) return FALSE;

    bpb->bytes_per_sector = iosys_get_word( sector + 0x0b );
    bpb->sectors_per_cluster = sector[0x0d];
    bpb->reserved_sectors = iosys_get_word( sector + 0x0e );
    bpb->fat_count = sector[0x10];
    bpb->root_entries = iosys_get_word( sector + 0x11 );
    total_sectors = iosys_get_word( sector + 0x13 );
    if (!total_sectors) total_sectors = iosys_get_dword( sector + 0x20 );
    bpb->total_sectors = total_sectors;
    bpb->media_descriptor = sector[0x15];
    bpb->sectors_per_fat = iosys_get_word( sector + 0x16 );

    if (!bpb->bytes_per_sector ||
        (bpb->bytes_per_sector & (bpb->bytes_per_sector - 1)) ||
        !bpb->sectors_per_cluster ||
        (bpb->sectors_per_cluster & (bpb->sectors_per_cluster - 1)) ||
        !bpb->reserved_sectors || !bpb->fat_count || bpb->fat_count > 2 ||
        !bpb->root_entries || !bpb->sectors_per_fat || !bpb->total_sectors)
        return FALSE;

    root_sectors = ((DWORD)bpb->root_entries * 32 + bpb->bytes_per_sector - 1) /
                   bpb->bytes_per_sector;
    first_data = bpb->reserved_sectors +
                 (DWORD)bpb->fat_count * bpb->sectors_per_fat + root_sectors;
    if (first_data >= bpb->total_sectors) return FALSE;

    data_sectors = bpb->total_sectors - first_data;
    clusters = data_sectors / bpb->sectors_per_cluster;

    /* 65525+ data clusters is FAT32 territory. */
    if (!clusters || clusters >= 65525) return FALSE;

    TRACE( "drive %c: FAT12/16 BPB: %u bytes/sector, %u sectors/cluster, "
           "%u reserved, %u FATs, %u root entries, %u sectors/FAT, %lu sectors\n",
           'A' + drive, bpb->bytes_per_sector, bpb->sectors_per_cluster,
           bpb->reserved_sectors, bpb->fat_count, bpb->root_entries,
           bpb->sectors_per_fat, bpb->total_sectors );
    return TRUE;
}

/***********************************************************************
 *           IOSYS_InitConfig
 *
 * Initialize the Win9x boot-directory environment from MSDOS.SYS.
 *
 * Windows 9x stores WinDir and WinBootDir in the [Paths] section and exposes
 * the resulting paths as the lowercase windir and winbootdir environment
 * variables. If MSDOS.SYS is absent, use the active Windows directory for
 * both values rather than inventing a separate boot path.
 *
 * IO.SYS interprets MSDOS.SYS before WIN.COM starts the protected-mode
 * Windows environment. KRNL386 therefore consumes the resulting path state;
 * boot policy such as BootGUI belongs to the Win9x boot owner and must not be
 * inferred from the Water host operating system here.
 */
static BOOL is_win3_standard_personality(void)
{
    char value[24];
    DWORD len = GetEnvironmentVariableA( WATER_WIN16_PERSONALITY_ENV, value, ARRAY_SIZE(value) );

    return len && len < ARRAY_SIZE(value) &&
           (!strcmp( value, WATER_WIN16_PERSONALITY_WIN30_STANDARD ) ||
            !strcmp( value, WATER_WIN16_PERSONALITY_WIN31_STANDARD ));
}

void IOSYS_InitConfig(void)
{
    char filename[MAX_PATH], windows[MAX_PATH];
    char windir[MAX_PATH], winbootdir[MAX_PATH], host_drive[16];
    DWORD len;
    BOOL have_file;

    /*
     * The guest personality owns this decision. Windows 3.x standard mode
     * and WIN386 use the pre-Win95 binary MSDOS.SYS model, while NT WOW has
     * no Win9x MSDOS.SYS boot policy. The default KRNL386 personality remains
     * the Win9x path.
     */
    if (is_win3_standard_personality() ||
        WIN386_QuerySession( NULL ) || kernel_is_nt_wow_session())
        return;

    len = GetWindowsDirectoryA( windows, sizeof(windows) );
    if (!len || len >= sizeof(windows))
        return;

    strcpy( windir, windows );
    strcpy( winbootdir, windows );

    iosys_boot_drive = 0;
    have_file = get_msdos_sys_path( filename );
    if (have_file)
    {
        get_msdos_path_value( filename, "WinDir", windows, windir, sizeof(windir) );
        get_msdos_path_value( filename, "WinBootDir", windir, winbootdir, sizeof(winbootdir) );
        get_msdos_path_value( filename, "HostWinBootDrv", "", host_drive, sizeof(host_drive) );

        /*
         * HostWinBootDrv is the Win9x boot-drive root, not the Windows
         * installation drive.  Preserve it as boot state so INT 21h and
         * CONFIG.SYS lookup do not silently follow GetWindowsDirectory().
         */
        iosys_boot_drive = drive_number_from_string( host_drive );
        if (!iosys_boot_drive) iosys_boot_drive = drive_number_from_string( filename );

        TRACE( "%s: WinDir=%s WinBootDir=%s HostWinBootDrv=%s boot drive=%u\n",
               debugstr_a(filename), debugstr_a(windir), debugstr_a(winbootdir),
               debugstr_a(host_drive), iosys_boot_drive );
    }
    else
        TRACE( "no boot-drive MSDOS.SYS, using Windows directory %s\n", debugstr_a(windows) );

    SetEnvironmentVariableA( "windir", windir );
    SetEnvironmentVariableA( "winbootdir", winbootdir );
}


/***********************************************************************
 *           IOSYS_GetBootDrive
 *
 * Return the boot drive using the DOS convention 1=A:, 2=B:, 3=C:.
 *
 * Water does not boot a real IO.SYS image yet, so the Windows directory is
 * the best available surrogate for the drive from which IO.SYS was loaded.
 */
BYTE IOSYS_GetBootDrive(void)
{
    WCHAR windows_directory[MAX_PATH];
    WCHAR current_directory[MAX_PATH];
    BYTE drive;
    UINT len;

    if (iosys_boot_drive) return iosys_boot_drive;

    len = GetWindowsDirectoryW( windows_directory, MAX_PATH );
    if (len >= 2 && len < MAX_PATH && windows_directory[1] == ':')
    {
        if (windows_directory[0] >= 'A' && windows_directory[0] <= 'Z')
            drive = windows_directory[0] - 'A';
        else if (windows_directory[0] >= 'a' && windows_directory[0] <= 'z')
            drive = windows_directory[0] - 'a';
        else
            drive = 26;

        if (drive < 26) return drive + 1;
    }

    if (GetCurrentDirectoryW( MAX_PATH, current_directory ) &&
        current_directory[1] == ':')
    {
        if (current_directory[0] >= 'A' && current_directory[0] <= 'Z')
            drive = current_directory[0] - 'A';
        else if (current_directory[0] >= 'a' && current_directory[0] <= 'z')
            drive = current_directory[0] - 'a';
        else
            drive = 26;

        if (drive < 26) return drive + 1;
    }

    return 3;
}


/***********************************************************************
 *           IOSYS_ReadConfigSys
 *
 * Parse the CONFIG.SYS directives for which Water already has concrete DOS
 * backing state.  Windows 9x IO.SYS owns this boot-time parse before WIN.COM.
 *
 * DEVICE/DEVICEHIGH, FILES, FCBS, STACKS, COUNTRY and SHELL require their own
 * real boot services and are deliberately not fabricated here.
 */
void IOSYS_ReadConfigSys(struct iosys_config_sys *config)
{
    static const DWORD max_config_size = 64 * 1024;
    char path[] = "C:\\CONFIG.SYS";
    char selected[72];
    HANDLE file;
    DWORD size, read, selected_len;
    char *buffer, *line;
    BYTE boot_drive;
    BOOL active = TRUE;

    if (!config) return;

    config->buffers_count = 15;
    config->buffers_lookahead = 1;
    config->last_drive = 0;
    config->umb_linked = -1;
    config->break_on = FALSE;

    selected_len = GetEnvironmentVariableA( "CONFIG", selected, sizeof(selected) );
    if (!selected_len || selected_len >= sizeof(selected)) selected[0] = 0;
    TRACE( "CONFIG.SYS selected block: %s\n",
           selected[0] ? debugstr_a(selected) : "(none)" );

    boot_drive = IOSYS_GetBootDrive();
    if (!boot_drive || boot_drive > 26) return;
    path[0] = 'A' + boot_drive - 1;

    file = CreateFileA( path, GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    if (file == INVALID_HANDLE_VALUE)
    {
        TRACE( "No %s; using DOS configuration defaults\n", path );
        return;
    }

    size = GetFileSize( file, NULL );
    if (size == INVALID_FILE_SIZE || size > max_config_size)
    {
        WARN( "Ignoring invalid or oversized %s\n", path );
        CloseHandle( file );
        return;
    }

    buffer = HeapAlloc( GetProcessHeap(), 0, size + 1 );
    if (!buffer)
    {
        CloseHandle( file );
        return;
    }

    if (!ReadFile( file, buffer, size, &read, NULL ))
    {
        HeapFree( GetProcessHeap(), 0, buffer );
        CloseHandle( file );
        return;
    }
    CloseHandle( file );
    buffer[read] = 0;

    line = buffer;
    while (*line)
    {
        char *next = strpbrk( line, "\r\n" );
        char *p = line, *name, *value, *end;
        long first, second;

        if (next)
        {
            *next++ = 0;
            while (*next == '\r' || *next == '\n') next++;
        }
        else next = line + strlen(line);

        while (*p == ' ' || *p == '\t') p++;
        end = p + strlen(p);
        while (end > p && (end[-1] == ' ' || end[-1] == '\t')) *--end = 0;

        if (!*p || *p == ';')
        {
            line = next;
            continue;
        }

        if (*p == '[')
        {
            end = strchr( p + 1, ']' );
            if (end)
            {
                char *section_end = end;

                while (section_end > p + 1 &&
                       (section_end[-1] == ' ' || section_end[-1] == '\t'))
                    section_end--;
                *section_end = 0;
                active = !_stricmp( p + 1, "common" ) ||
                         (selected[0] && !_stricmp( p + 1, selected ));
            }
            else active = FALSE;
            line = next;
            continue;
        }

        if (!active)
        {
            line = next;
            continue;
        }

        name = p;
        while (*p && *p != '=' && *p != ';' && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = 0;
        while (*p == '=' || *p == ';' || *p == ' ' || *p == '\t') p++;
        value = p;

        if (!_stricmp( name, "REM" ))
        {
            line = next;
            continue;
        }

        if (!_stricmp( name, "BREAK" ))
        {
            if (!_stricmp( value, "ON" )) config->break_on = TRUE;
            else if (!_stricmp( value, "OFF" )) config->break_on = FALSE;
        }
        else if (!_stricmp( name, "BUFFERS" ) || !_stricmp( name, "BUFFERSHIGH" ))
        {
            first = strtol( value, &end, 10 );
            if (first >= 1 && first <= 99)
            {
                config->buffers_count = first;
                while (*end == ' ' || *end == '\t') end++;
                if (*end == ',')
                {
                    second = strtol( end + 1, &end, 10 );
                    if (second >= 0 && second <= 8)
                        config->buffers_lookahead = second;
                }
            }
        }
        else if (!_stricmp( name, "LASTDRIVE" ) || !_stricmp( name, "LASTDRIVEHIGH" ))
        {
            while (*value == ' ' || *value == '\t') value++;
            if ((value[0] >= 'A' && value[0] <= 'Z') ||
                (value[0] >= 'a' && value[0] <= 'z'))
                config->last_drive = (value[0] & ~0x20) - 'A' + 1;
        }
        else if (!_stricmp( name, "DOS" ))
        {
            char *token = value;

            while (*token)
            {
                char *token_end;

                while (*token == ' ' || *token == '\t' || *token == ',') token++;
                token_end = token;
                while (*token_end && *token_end != ',' &&
                       *token_end != ' ' && *token_end != '\t') token_end++;

                if ((token_end - token) == 3 && !_strnicmp( token, "UMB", 3 ))
                    config->umb_linked = TRUE;
                else if ((token_end - token) == 5 && !_strnicmp( token, "NOUMB", 5 ))
                    config->umb_linked = FALSE;

                token = token_end;
            }
        }

        line = next;
    }

    TRACE( "CONFIG.SYS: BUFFERS=%u,%u LASTDRIVE=%u UMB=%d BREAK=%u\n",
           config->buffers_count, config->buffers_lookahead,
           config->last_drive, config->umb_linked, config->break_on );
    HeapFree( GetProcessHeap(), 0, buffer );
}
