/*
 * Windows 9x IO.SYS compatibility ownership.
 *
 * These interfaces model boot state that must eventually be produced by the
 * real-mode IO.SYS image before WIN.COM starts.
 */
#ifndef __WATER_IO_SYS_H
#define __WATER_IO_SYS_H

#include "windef.h"

struct iosys_config_sys
{
    WORD buffers_count;
    WORD buffers_lookahead;
    BYTE last_drive;
    INT umb_linked;       /* -1 when CONFIG.SYS did not specify UMB/NOUMB */
    BOOL break_on;

    /* Requested by CONFIG.SYS only; this does not mean ANSI.SYS is loaded. */
    BOOL ansi_device_requested;
    BOOL ansi_device_high;
    BOOL ansi_switch_extended_keys;   /* ANSI.SYS /X */
    BOOL ansi_switch_legacy_keys;     /* ANSI.SYS /K */
    BOOL ansi_switch_readable_scroll; /* ANSI.SYS /R */

    /* SWITCHES= is IO.SYS boot policy, not an INT 21h runtime service. */
    BOOL switches_fast_boot;           /* /F: suppress startup F5/F8 delay */
    BOOL switches_legacy_keyboard;     /* /K: conventional keyboard behavior */
    BOOL switches_disable_boot_keys;   /* /N: disallow F5/F8 boot bypass */
    BOOL switches_wina20_moved;        /* /W: historical Windows 3.x support */
    INT switches_ebios_bytes;          /* /E: -1 absent, 0 disable, >0 relocation bytes */
};

/* BIOS MBR -> active primary partition -> volume boot record coordinates.
 * No raw device reads, writes or guest control transfer occur here. */
struct iosys_mbr_boot_partition
{
    BYTE slot;                /* primary partition index, 0..3 */
    BYTE partition_type;
    DWORD start_lba;         /* absolute 512-byte disk sectors */
    DWORD sector_count;
};

BOOL IOSYS_ParseMbrBootPartition(const BYTE mbr[512], ULONGLONG disk_sectors,
                                 struct iosys_mbr_boot_partition *out);
BOOL IOSYS_IsWin95RtmPartitionType(BYTE type);
BOOL IOSYS_ValidateWin95RtmVbr(const BYTE vbr[512],
                               const struct iosys_mbr_boot_partition *part);

/* Classic FAT12/FAT16 BIOS parameter block used by DOS/IO.SYS. */
struct iosys_fat_bpb
{
    WORD bytes_per_sector;
    BYTE sectors_per_cluster;
    WORD reserved_sectors;
    BYTE fat_count;
    WORD root_entries;
    DWORD total_sectors;
    BYTE media_descriptor;
    WORD sectors_per_fat;
};

/* MSDOS.SYS [Options] DoubleBuffer: 0=default/off, 1=conditional, 2=forced.
 * This is file-requested boot policy only, not evidence that DBLBUFF.SYS loaded. */
BOOL IOSYS_ParseDoubleBuffer(const char *value, BYTE *mode);
BYTE IOSYS_GetMsdosDoubleBufferMode(void);

void IOSYS_InitConfig(void);
BYTE IOSYS_GetBootDrive(void);
BOOL IOSYS_GetFat1216BPB(BYTE drive, struct iosys_fat_bpb *bpb);

/* Extended DOS 7.1 FAT32 BPB: unlike FAT12/16, its root is a cluster
 * chain and FAT length occupies a 32-bit field.  This is read-only
 * device metadata, not a host file-system emulation or Win32 ABI. */
struct iosys_fat32_bpb
{
    WORD bytes_per_sector;
    BYTE sectors_per_cluster;
    WORD reserved_sectors;
    BYTE fat_count;
    DWORD total_sectors;
    BYTE media_descriptor;
    DWORD sectors_per_fat;
    WORD mirroring_flags;
    WORD fs_version;
    DWORD root_cluster;
    WORD info_sector;
    WORD backup_boot_sector;
    DWORD first_data_sector;
    DWORD data_clusters;
};

BOOL IOSYS_ParseFat32BPB(const BYTE sector[512], struct iosys_fat32_bpb *bpb);
BOOL IOSYS_GetFat32BPB(BYTE drive, struct iosys_fat32_bpb *bpb);
BOOL IOSYS_ParseSwitches(const char *value, struct iosys_config_sys *config);
BOOL IOSYS_ParseAnsiDevice(const char *value, struct iosys_config_sys *config, BOOL high);
void IOSYS_ReadConfigSys(struct iosys_config_sys *config);

#endif /* __WATER_IO_SYS_H */
