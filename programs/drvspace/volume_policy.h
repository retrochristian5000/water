/*
 * DriveSpace compression target eligibility.
 *
 * This interface is a read-only preflight boundary, not a compressor,
 * DriveSpace device driver, or a license to change any guest volume.
 */
#ifndef __WATER_DRIVESPACE_VOLUME_POLICY_H
#define __WATER_DRIVESPACE_VOLUME_POLICY_H

#include "windef.h"

enum drivespace_target_kind
{
    DRIVESPACE_TARGET_UNKNOWN,              /* Missing/invalid/other filesystem */
    DRIVESPACE_TARGET_FAT32_REJECTED,       /* Recognized FAT32: never compress */
    DRIVESPACE_TARGET_FAT12_16_CANDIDATE    /* Geometry recognized; not yet safe to modify */
};

/* Drive index is zero-based: 0=A:, 1=B:, ... .
 * A candidate still requires CVF, write-safety, and guest boot checks. */
enum drivespace_target_kind DRIVESPACE_ClassifyTarget(BYTE drive);

#endif /* __WATER_DRIVESPACE_VOLUME_POLICY_H */
