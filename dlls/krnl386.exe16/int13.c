/*
 * BIOS interrupt 13h handler
 *
 * Copyright 1997 Andreas Mohr
 * Copyright 2026 Water contributors
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

#include <stdlib.h>

#include "dosexe.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

struct int13_drive
{
    BYTE dos_drive;
    DWORD cylinders;
    DWORD heads;
    DWORD sectors_per_track;
};

static BYTE int13_last_status;

static void int13_set_status(I386_CONTEXT *context, BYTE status)
{
    int13_last_status = status;
    SET_AH(context, status);

    if (status)
        SET_CFLAG(context);
    else
        RESET_CFLAG(context);
}

static unsigned int int13_drive_count(BOOL fixed)
{
    WCHAR root[] = {'A', ':', '\\', 0};
    unsigned int count = 0, i;
    UINT type;

    for (i = fixed ? 2 : 0; i < (fixed ? MAX_DOS_DRIVES : 2); ++i)
    {
        root[0] = 'A' + i;
        type = GetDriveTypeW(root);

        if (fixed)
        {
            if (type == DRIVE_FIXED)
                ++count;
        }
        else if (type != DRIVE_UNKNOWN && type != DRIVE_NO_ROOT_DIR)
            ++count;
    }

    return count;
}

static BOOL int13_get_drive(BYTE bios_drive, struct int13_drive *drive)
{
    WCHAR root[] = {'A', ':', '\\', 0};
    DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
    ULONGLONG total_sectors;
    unsigned int target, found = 0, i;
    UINT type;

    if (bios_drive < 0x80)
    {
        if (bios_drive > 1)
            return FALSE;
        drive->dos_drive = bios_drive;
    }
    else
    {
        target = bios_drive - 0x80;
        drive->dos_drive = 0xff;

        for (i = 2; i < MAX_DOS_DRIVES; ++i)
        {
            root[0] = 'A' + i;
            if (GetDriveTypeW(root) != DRIVE_FIXED)
                continue;
            if (found++ == target)
            {
                drive->dos_drive = i;
                break;
            }
        }

        if (drive->dos_drive == 0xff)
            return FALSE;
    }

    root[0] = 'A' + drive->dos_drive;
    type = GetDriveTypeW(root);
    if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
        return FALSE;

    if (bios_drive < 0x80)
    {
        drive->heads = 2;
        drive->sectors_per_track = 18;
    }
    else
    {
        drive->heads = 255;
        drive->sectors_per_track = 63;
    }

    if (GetDiskFreeSpaceW(root, &sectors_per_cluster, &bytes_per_sector,
            &free_clusters, &total_clusters))
    {
        total_sectors = (ULONGLONG)sectors_per_cluster * total_clusters;

        if (bios_drive < 0x80)
        {
            if (total_sectors <= 720 * 1024 / 512)
                drive->sectors_per_track = 9;
            else if (total_sectors <= 1200 * 1024 / 512)
                drive->sectors_per_track = 15;
            else if (total_sectors <= 1440 * 1024 / 512)
                drive->sectors_per_track = 18;
            else
                drive->sectors_per_track = 36;
        }

        drive->cylinders = total_sectors / (drive->heads * drive->sectors_per_track);
    }
    else
    {
        drive->cylinders = bios_drive < 0x80 ? 80 : 1024;
    }

    if (!drive->cylinders)
        drive->cylinders = 1;
    else if (drive->cylinders > 1024)
        drive->cylinders = 1024;

    return TRUE;
}

static BOOL int13_get_lba(I386_CONTEXT *context, const struct int13_drive *drive, DWORD *lba)
{
    DWORD cylinder = CH_reg(context) | ((CL_reg(context) & 0xc0) << 2);
    DWORD sector = CL_reg(context) & 0x3f;
    DWORD head = DH_reg(context);

    if (!sector || sector > drive->sectors_per_track ||
        head >= drive->heads || cylinder >= drive->cylinders)
        return FALSE;

    *lba = ((cylinder * drive->heads) + head) * drive->sectors_per_track + sector - 1;
    return TRUE;
}

static BYTE *int13_get_buffer(I386_CONTEXT *context)
{
    if (!context->SegEs)
        return (BYTE *)(UINT_PTR)context->Ebx;

    return ldt_get_ptr(context->SegEs, BX_reg(context));
}

static void int13_get_drive_parameters(I386_CONTEXT *context)
{
    struct int13_drive drive;
    DWORD max_cylinder;

    if (!int13_get_drive(DL_reg(context), &drive))
    {
        int13_set_status(context, 0x07);
        return;
    }

    max_cylinder = drive.cylinders - 1;
    SET_CH(context, max_cylinder & 0xff);
    SET_CL(context, drive.sectors_per_track | ((max_cylinder >> 2) & 0xc0));
    SET_DH(context, drive.heads - 1);
    SET_DL(context, int13_drive_count(DL_reg(context) >= 0x80));
    SET_BL(context, DL_reg(context) < 0x80 ? 4 : 0);
    int13_set_status(context, 0x00);
}

static void int13_transfer(I386_CONTEXT *context, BOOL write)
{
    struct int13_drive drive;
    BYTE *buffer;
    DWORD lba;
    BYTE count = AL_reg(context);
    BOOL ret;

    if (!count || !int13_get_drive(DL_reg(context), &drive) ||
        !int13_get_lba(context, &drive, &lba))
    {
        SET_AL(context, 0);
        int13_set_status(context, 0x04);
        return;
    }

    buffer = int13_get_buffer(context);
    if (!buffer)
    {
        SET_AL(context, 0);
        int13_set_status(context, 0x09);
        return;
    }

    if (write)
        ret = DOSVM_RawWrite(drive.dos_drive, lba, count, buffer, FALSE);
    else
        ret = DOSVM_RawRead(drive.dos_drive, lba, count, buffer, FALSE);

    if (!ret)
    {
        SET_AL(context, 0);
        int13_set_status(context, 0x20);
        return;
    }

    SET_AL(context, count);
    int13_set_status(context, 0x00);
}

void WINAPI DOSVM_Int13Handler(I386_CONTEXT *context)
{
    struct int13_drive drive;

    TRACE("AH=%02x DL=%02x\n", AH_reg(context), DL_reg(context));

    switch (AH_reg(context))
    {
    case 0x00: /* reset disk system */
        int13_set_status(context, 0x00);
        break;

    case 0x01: /* status of disk system */
        int13_set_status(context, int13_last_status);
        break;

    case 0x02: /* read sectors into memory */
        int13_transfer(context, FALSE);
        break;

    case 0x03: /* write sectors from memory */
        int13_transfer(context, TRUE);
        break;

    case 0x04: /* verify disk sectors */
        {
            DWORD lba;

            if (int13_get_drive(DL_reg(context), &drive) &&
                int13_get_lba(context, &drive, &lba))
            {
                int13_set_status(context, 0x00);
                break;
            }
        }
        SET_AL(context, 0);
        int13_set_status(context, 0x04);
        break;

    case 0x05: /* format track */
    case 0x06: /* format track and set bad-sector flags */
    case 0x07: /* format drive */
        int13_set_status(context, 0x0c);
        break;

    case 0x08: /* get drive parameters */
        int13_get_drive_parameters(context);
        break;

    case 0x09: /* initialize controller */
    case 0x0c: /* seek */
    case 0x0d: /* alternate reset */
    case 0x10: /* check drive ready */
    case 0x11: /* recalibrate */
    case 0x14: /* controller diagnostic */
    case 0x19: /* park heads */
        int13_set_status(context, int13_get_drive(DL_reg(context), &drive) ? 0x00 : 0x01);
        break;

    case 0x0a: /* read long */
    case 0x0b: /* write long */
    case 0x0e: /* read sector buffer */
    case 0x0f: /* write sector buffer */
    case 0x12: /* controller RAM diagnostic */
    case 0x13: /* drive diagnostic */
        int13_set_status(context, 0x01);
        break;

    case 0x15: /* get disk type */
        if (!int13_get_drive(DL_reg(context), &drive))
        {
            int13_set_status(context, 0x01);
            break;
        }

        int13_set_status(context, 0x00);
        SET_AH(context, DL_reg(context) >= 0x80 ? 0x03 : 0x02);
        break;

    case 0x16: /* floppy disk change status */
        int13_set_status(context, DL_reg(context) < 2 && int13_get_drive(DL_reg(context), &drive) ? 0x00 : 0x01);
        break;

    case 0x17: /* set disk type for format */
    case 0x18: /* set media type for format */
        int13_set_status(context, DL_reg(context) < 2 && int13_get_drive(DL_reg(context), &drive) ? 0x00 : 0x01);
        break;

    default:
        INT_BARF(context, 0x13);
        int13_set_status(context, 0x01);
        break;
    }
}
