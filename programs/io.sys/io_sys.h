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
void IOSYS_ReadConfigSys(struct iosys_config_sys *config);

#endif /* __WATER_IO_SYS_H */
