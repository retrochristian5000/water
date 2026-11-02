/*
 * DriveSpace read-only target policy.
 *
 * Windows 98 DriveSpace 3 detects FAT32 but cannot compress FAT32 volumes.
 * Keep that negative capability independent of DOS 7.1 FAT32 support in
 * IO.SYS, ScanDisk, FAT32 disk services, and the host filesystem.
 */
#include "volume_policy.h"
#include "../io.sys/io_sys.h"

/* IO.SYS owns raw guest BPB reads and validates the FAT geometry. Never
 * infer a DOS volume's format from a host file path or fabricated DPB.
 * Prioritize FAT32 rejection even if two decoders were to disagree. */
enum drivespace_target_kind DRIVESPACE_ClassifyTarget(BYTE drive)
{
    struct iosys_fat32_bpb fat32;
    struct iosys_fat_bpb fat1216;

    if (drive >= 26) return DRIVESPACE_TARGET_UNKNOWN;

    if (IOSYS_GetFat32BPB(drive, &fat32))
        return DRIVESPACE_TARGET_FAT32_REJECTED;

    if (IOSYS_GetFat1216BPB(drive, &fat1216))
        return DRIVESPACE_TARGET_FAT12_16_CANDIDATE;

    return DRIVESPACE_TARGET_UNKNOWN;
}
