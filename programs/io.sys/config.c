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

/* Boot state owned by IO.SYS, not by the 32-bit KERNEL32/GDI32 loaders. */
static BYTE iosys_boot_drive;
/* File policy only: registry WinBoot overrides and controller detection are
 * separate boot inputs that are not yet implemented in Water. */
static BYTE iosys_msdos_double_buffer_mode;

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

/*
 * MSDOS.SYS [Options] DoubleBuffer is a three-state value, not a Boolean:
 * 0 = no file-requested double buffering (default), 1 = conditional,
 * 2 = unconditional. Reject malformed values without updating the caller.
 * This parser only records policy; it does not load DBLBUFF.SYS.
 */
BOOL IOSYS_ParseDoubleBuffer(const char *value, BYTE *mode)
{
    const char *p = value;
    BYTE parsed;

    if (!p || !mode) return FALSE;
    while (*p == ' ' || *p == '\t') p++;
    if (*p < '0' || *p > '2') return FALSE;
    parsed = *p++ - '0';
    while (*p == ' ' || *p == '\t') p++;
    if (*p) return FALSE;
    *mode = parsed;
    return TRUE;
}

BYTE IOSYS_GetMsdosDoubleBufferMode(void)
{
    return iosys_msdos_double_buffer_mode;
}

static WORD iosys_get_word( const BYTE *p )
{
    return p[0] | (p[1] << 8);
}

static DWORD iosys_get_dword( const BYTE *p )
{
    return iosys_get_word( p ) | ((DWORD)iosys_get_word( p + 2 ) << 16);
}



/*
 * Decode the four-entry PC/AT MBR to obtain the single active partition's
 * volume boot record (VBR) location.  This never executes the MBR and never
 * reads or writes host sectors; the caller provides sector 0 and the actual
 * disk size in 512-byte logical sectors.
 *
 * MBR coordinates are DISK LBAs.  They must not be confused with the volume
 * sector zero returned by \\.\\C: or other DOS logical-drive mappings.
 */
BOOL IOSYS_ParseMbrBootPartition(const BYTE mbr[512], ULONGLONG disk_sectors,
                                 struct iosys_mbr_boot_partition *out)
{
    struct iosys_mbr_boot_partition parsed;
    unsigned int i;
    BOOL found = FALSE;

    if (!mbr || !out || disk_sectors < 2 ||
        mbr[510] != 0x55 || mbr[511] != 0xaa)
        return FALSE;

    memset( &parsed, 0, sizeof(parsed) );
    for (i = 0; i < 4; i++)
    {
        const BYTE *entry = mbr + 0x1be + i * 16;
        BYTE indicator = entry[0];
        DWORD start, count;

        /* A malformed boot flag invalidates the table even when some other
         * entry is active: do not pick an arbitrary partition. */
        if (indicator != 0x00 && indicator != 0x80) return FALSE;
        if (indicator != 0x80) continue;
        if (found) return FALSE;

        start = iosys_get_dword( entry + 8 );
        count = iosys_get_dword( entry + 12 );
        if (!entry[4] || !start || !count ||
            (ULONGLONG)start + count > disk_sectors)
            return FALSE;

        parsed.slot = i;
        parsed.partition_type = entry[4];
        parsed.start_lba = start;
        parsed.sector_count = count;
        found = TRUE;
    }

    if (!found) return FALSE;
    *out = parsed;
    return TRUE;
}

/*
 * Recognize the FAT12 and FAT16 primary partitions available on Windows 95
 * RTM.  FAT32 types 0x0b/0x0c arrived with OSR2, not the RTM release.
 * Type 0x0e (FAT16 LBA) also requires BIOS INT 13h extensions, which
 * Water's current boot-chain model does not implement.
 */
BOOL IOSYS_IsWin95RtmPartitionType(BYTE type)
{
    return type == 0x01 || type == 0x04 || type == 0x06 || type == 0x0e;
}

/* Given an independently supplied 512-byte VBR at the selected MBR LBA,
 * check FAT12/16 geometry before handing off to a future IO.SYS loader.
 * This is structural validation; it does NOT authenticate or execute boot
 * code, check the FAT/root directory, or prove IO.SYS exists on the volume.
 */
BOOL IOSYS_ValidateWin95RtmVbr(const BYTE vbr[512],
                               const struct iosys_mbr_boot_partition *part)
{
    DWORD total, first_data, root_sectors, clusters;
    WORD reserved, root_entries, fat_sectors;
    BYTE sectors_per_cluster, fat_count;
    ULONGLONG fat_entries;
    BOOL fat12;

    if (!vbr || !part || !IOSYS_IsWin95RtmPartitionType(part->partition_type) ||
        vbr[510] != 0x55 || vbr[511] != 0xaa ||
        !((vbr[0] == 0xeb && vbr[2] == 0x90) || vbr[0] == 0xe9) ||
        iosys_get_word( vbr + 0x0b ) != 512)
        return FALSE;

    sectors_per_cluster = vbr[0x0d];
    reserved = iosys_get_word( vbr + 0x0e );
    fat_count = vbr[0x10];
    root_entries = iosys_get_word( vbr + 0x11 );
    fat_sectors = iosys_get_word( vbr + 0x16 );
    total = iosys_get_word( vbr + 0x13 );
    if (!total) total = iosys_get_dword( vbr + 0x20 );

    if (!sectors_per_cluster || sectors_per_cluster > 128 ||
        (sectors_per_cluster & (sectors_per_cluster - 1)) ||
        !reserved || !fat_count || fat_count > 2 || !root_entries ||
        !fat_sectors || !total || total > part->sector_count ||
        iosys_get_dword( vbr + 0x1c ) != part->start_lba ||
        vbr[0x15] < 0xf0)
        return FALSE;

    root_sectors = ((DWORD)root_entries * 32 + 511) / 512;
    first_data = reserved + (DWORD)fat_count * fat_sectors + root_sectors;
    if (first_data >= total) return FALSE;

    clusters = (total - first_data) / sectors_per_cluster;
    if (!clusters || clusters >= 65525) return FALSE;
    fat12 = clusters < 4085;
    if (fat12 != (part->partition_type == 0x01)) return FALSE;

    fat_entries = (ULONGLONG)fat_sectors * 512;
    fat_entries = fat12 ? fat_entries * 2 / 3 : fat_entries / 2;
    if (fat_entries < (ULONGLONG)clusters + 2) return FALSE;
    return TRUE;
}

/*
 * Read a physical volume boot sector only when the host grants access.
 * Never write physical sectors, and never invent a synthetic FAT image
 * for APFS/ext4/NTFS-backed Wine drive mappings.
 */
static BOOL iosys_read_boot_sector(BYTE drive, BYTE sector[512])
{
    WCHAR volume[] = {'\\','\\','.','\\','A',':',0};
    DWORD read = 0;
    HANDLE file;
    BOOL ret;

    if (drive >= 26) return FALSE;
    volume[4] += drive;
    file = CreateFileW( volume, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL );
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    ret = ReadFile( file, sector, 512, &read, NULL ) && read == 512;
    CloseHandle( file );
    return ret;
}

/* Pure decoder for a 512-byte FAT32 boot-sector prefix.  Reject invalid
 * layouts before allowing untrusted BPB values into guest-visible DOS DPBs.
 * The FAT32 filesystem type is determined from BPB geometry, not the
 * optional, untrusted "FAT32   " text field. */
BOOL IOSYS_ParseFat32BPB(const BYTE sector[512], struct iosys_fat32_bpb *out)
{
    struct iosys_fat32_bpb bpb;
    ULONGLONG data_start, fat_entries;
    DWORD clusters;

    if (!sector || !out || sector[510] != 0x55 || sector[511] != 0xaa)
        return FALSE;
    memset( &bpb, 0, sizeof(bpb) );
    bpb.bytes_per_sector = iosys_get_word( sector + 0x0b );
    bpb.sectors_per_cluster = sector[0x0d];
    bpb.reserved_sectors = iosys_get_word( sector + 0x0e );
    bpb.fat_count = sector[0x10];
    bpb.total_sectors = iosys_get_dword( sector + 0x20 );
    bpb.media_descriptor = sector[0x15];
    bpb.sectors_per_fat = iosys_get_dword( sector + 0x24 );
    bpb.mirroring_flags = iosys_get_word( sector + 0x28 );
    bpb.fs_version = iosys_get_word( sector + 0x2a );
    bpb.root_cluster = iosys_get_dword( sector + 0x2c );
    bpb.info_sector = iosys_get_word( sector + 0x30 );
    bpb.backup_boot_sector = iosys_get_word( sector + 0x32 );

    if (bpb.bytes_per_sector < 512 || bpb.bytes_per_sector > 4096 ||
        (bpb.bytes_per_sector & (bpb.bytes_per_sector - 1)) ||
        !bpb.sectors_per_cluster || bpb.sectors_per_cluster > 128 ||
        (bpb.sectors_per_cluster & (bpb.sectors_per_cluster - 1)) ||
        (DWORD)bpb.bytes_per_sector * bpb.sectors_per_cluster > 65536 ||
        !bpb.reserved_sectors || !bpb.fat_count || bpb.fat_count > 2 ||
        !bpb.total_sectors || !bpb.sectors_per_fat ||
        bpb.media_descriptor < 0xf0 ||
        bpb.fs_version ||
        iosys_get_word( sector + 0x11 ) || /* FAT32 has no fixed root dir */
        iosys_get_word( sector + 0x13 ) || /* FAT32 uses TotSec32 */
        iosys_get_word( sector + 0x16 ) || /* FAT32 uses FATSz32 */
        (bpb.mirroring_flags & 0xff70) || /* reserved flags must be zero */
        ((bpb.mirroring_flags & 0x80) &&
         (bpb.mirroring_flags & 0x0f) >= bpb.fat_count) ||
        (bpb.info_sector != 0xffff &&
         bpb.info_sector >= bpb.reserved_sectors) ||
        (bpb.backup_boot_sector != 0xffff &&
         bpb.backup_boot_sector >= bpb.reserved_sectors))
        return FALSE;

    data_start = (ULONGLONG)bpb.reserved_sectors +
                 (ULONGLONG)bpb.fat_count * bpb.sectors_per_fat;
    if (data_start >= bpb.total_sectors) return FALSE;

    clusters = (bpb.total_sectors - (DWORD)data_start) / bpb.sectors_per_cluster;
    if (clusters < 65525 || clusters > 0x0ffffff5 ||
        bpb.root_cluster < 2 || bpb.root_cluster > clusters + 1)
        return FALSE;

    fat_entries = (ULONGLONG)bpb.sectors_per_fat * bpb.bytes_per_sector / 4;
    if (fat_entries < (ULONGLONG)clusters + 2) return FALSE;

    bpb.first_data_sector = (DWORD)data_start;
    bpb.data_clusters = clusters;
    *out = bpb;
    return TRUE;
}

BOOL IOSYS_GetFat32BPB(BYTE drive, struct iosys_fat32_bpb *bpb)
{
    BYTE sector[512];
    if (!bpb || !iosys_read_boot_sector( drive, sector )) return FALSE;
    return IOSYS_ParseFat32BPB( sector, bpb );
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
    BYTE sector[512];
    DWORD total_sectors, root_sectors, first_data, data_sectors, clusters;

    if (!bpb || !iosys_read_boot_sector( drive, sector )) return FALSE;

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
    struct water_win16_profile profile;

    water_win16_read_profile( &profile );
    return profile.line == WATER_WIN16_LINE_PRE9X_FAMILY &&
           profile.mode == WATER_WIN16_MODE_STANDARD_FAMILY;
}

static BOOL is_winme_personality(void)
{
    struct water_win16_profile profile;

    water_win16_read_profile( &profile );
    return profile.line == WATER_WIN16_LINE_WIN9X_FAMILY &&
           profile.major == 4 && profile.minor >= 90;
}

void IOSYS_InitConfig(void)
{
    char filename[MAX_PATH], windows[MAX_PATH];
    char windir[MAX_PATH], winbootdir[MAX_PATH], host_drive[16];
    char double_buffer_value[32];
    DWORD len;
    BOOL have_file;

    /*
     * The guest personality owns this decision. Windows 3.x standard mode
     * and WIN386 use the pre-Win95 binary MSDOS.SYS model, while NT WOW has
     * no Win9x MSDOS.SYS boot policy. The default KRNL386 personality remains
     * the Win9x path.
     */
    iosys_msdos_double_buffer_mode = 0; /* No stale boot policy across initializations. */
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

        /* Read the DOS 7 boot policy without trying to load a real-mode
         * driver from the KRNL386 compatibility bridge. Windows Me's
         * reduced real-mode startup is a different, unmodeled path.
         */
        if (!is_winme_personality())
        {
            len = GetPrivateProfileStringA( "Options", "DoubleBuffer", "0",
                                            double_buffer_value, sizeof(double_buffer_value),
                                            filename );
            if (len >= sizeof(double_buffer_value) - 1 ||
                !IOSYS_ParseDoubleBuffer( double_buffer_value, &iosys_msdos_double_buffer_mode ))
                WARN( "Ignoring invalid DoubleBuffer setting in %s\n", debugstr_a(filename) );
        }

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


/*
 * Parse MS-DOS/Win9x CONFIG.SYS SWITCHES settings.  /W is documented
 * for DOS 6.x and /E[:n] for Win98; retain their separate meanings in
 * IO.SYS state until version-specific real-mode boot policy is implemented.
 *
 * These switches affect actions before WIN.COM and KRNL386 run, so merely
 * caching them in the compatibility bridge does not simulate their effects.
 * An invalid directive must not partially update the boot configuration.
 */
BOOL IOSYS_ParseSwitches(const char *value, struct iosys_config_sys *config)
{
    struct iosys_config_sys parsed;
    const char *p;

    if (!value || !config) return FALSE;
    parsed = *config;
    p = value;

    while (*p)
    {
        char option;

        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        if (*p++ != '/' || !*p) return FALSE;
        option = *p++;
        if (option >= 'a' && option <= 'z') option -= 'a' - 'A';
        switch (option)
        {
        case 'F': parsed.switches_fast_boot = TRUE; break;
        case 'K': parsed.switches_legacy_keyboard = TRUE; break;
        case 'N': parsed.switches_disable_boot_keys = TRUE; break;
        case 'W': parsed.switches_wina20_moved = TRUE; break;
        case 'E':
            if (*p == ':')
            {
                unsigned int size = 0;

                p++;
                if (*p < '0' || *p > '9') return FALSE;
                while (*p >= '0' && *p <= '9')
                {
                    size = size * 10 + *p++ - '0';
                    if (size > 1024) return FALSE;
                }
                if (size < 48) return FALSE;
                parsed.switches_ebios_bytes = (size + 15) & ~15;
            }
            else parsed.switches_ebios_bytes = 0;
            break;
        default:
            return FALSE;
        }
        if (*p && *p != '/' && *p != ' ' && *p != '\t') return FALSE;
    }

    *config = parsed;
    return TRUE;
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
    config->switches_fast_boot = FALSE;
    config->switches_legacy_keyboard = FALSE;
    config->switches_disable_boot_keys = FALSE;
    config->switches_wina20_moved = FALSE;
    config->switches_ebios_bytes = -1;

    /*
     * Normal Windows Me hard-disk startup does not execute the DOS boot
     * directives that earlier Win9x consumes from CONFIG.SYS.  Environment
     * import is a separate Me compatibility path and is intentionally not
     * represented by these BUFFERS/LASTDRIVE/DOS/BREAK fields.
     */
    if (is_winme_personality())
    {
        TRACE( "Windows Me personality: ignoring CONFIG.SYS DOS boot directives\n" );
        return;
    }

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
        else if (!_stricmp( name, "SWITCHES" ))
        {
            if (!IOSYS_ParseSwitches( value, config ))
                WARN( "Ignoring invalid SWITCHES directive in %s: %s\n", path, debugstr_a(value) );
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

    TRACE( "CONFIG.SYS: BUFFERS=%u,%u LASTDRIVE=%u UMB=%d BREAK=%u "
           "SWITCHES=/F:%u /K:%u /N:%u /W:%u /E:%d\n",
           config->buffers_count, config->buffers_lookahead,
           config->last_drive, config->umb_linked, config->break_on,
           config->switches_fast_boot, config->switches_legacy_keyboard,
           config->switches_disable_boot_keys, config->switches_wina20_moved,
           config->switches_ebios_bytes );
    HeapFree( GetProcessHeap(), 0, buffer );
}
