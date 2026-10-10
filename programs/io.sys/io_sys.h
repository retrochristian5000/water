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
};

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
void IOSYS_ReadConfigSys(struct iosys_config_sys *config);

#endif /* __WATER_IO_SYS_H */
