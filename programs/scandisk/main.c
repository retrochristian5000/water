/*
 * ScanDisk-compatible diagnostic utility
 *
 * Copyright 2026 Water project contributors
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

#include <windows.h>
#include <winioctl.h>
#include <stdio.h>
#include <string.h>

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(scandisk);

#define FAT12_MAX_CLUSTERS 4084u
#define FAT16_MAX_CLUSTERS 65524u

#define FAT32_FSINFO_LEAD_SIG   0x41615252u
#define FAT32_FSINFO_STRUC_SIG  0x61417272u
#define FAT32_FSINFO_TRAIL_SIG  0xaa550000u

#define VOLUME_DIRTY_FLAG 0x00000001u

enum fat_kind
{
    FAT_KIND_NONE,
    FAT_KIND_12,
    FAT_KIND_16,
    FAT_KIND_32
};

struct scan_options
{
    BOOL all;
    BOOL autofix;
    BOOL checkonly;
    BOOL custom;
    BOOL nosave;
    BOOL nosummary;
    BOOL surface;
    BOOL mono;
    BOOL help;
};

struct scan_result
{
    unsigned int problems;
    unsigned int incomplete;
};

struct fat_layout
{
    enum fat_kind kind;
    DWORD bytes_per_sector;
    DWORD sectors_per_cluster;
    DWORD reserved_sectors;
    DWORD fat_count;
    DWORD root_entries;
    DWORD total_sectors;
    DWORD sectors_per_fat;
    DWORD root_dir_sectors;
    DWORD fsinfo_sector;
    DWORD backup_boot_sector;
    DWORD ext_flags;
    ULONGLONG data_sectors;
    ULONGLONG clusters;
};

static WORD get_u16(const BYTE *p)
{
    return (WORD)(p[0] | ((WORD)p[1] << 8));
}

static DWORD get_u32(const BYTE *p)
{
    return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16) | ((DWORD)p[3] << 24);
}

static BOOL is_power_of_two(DWORD value)
{
    return value && !(value & (value - 1));
}

static const WCHAR *fat_name(enum fat_kind kind)
{
    switch (kind)
    {
    case FAT_KIND_12: return L"FAT12";
    case FAT_KIND_16: return L"FAT16";
    case FAT_KIND_32: return L"FAT32";
    default: return L"unknown";
    }
}

static BOOL read_at(HANDLE handle, ULONGLONG offset, void *buffer, DWORD size)
{
    LARGE_INTEGER pos;
    DWORD read;

    pos.QuadPart = offset;
    if (!SetFilePointerEx(handle, pos, NULL, FILE_BEGIN)) return FALSE;
    return ReadFile(handle, buffer, size, &read, NULL) && read == size;
}

static BOOL is_local_drive(UINT type)
{
    return type == DRIVE_FIXED || type == DRIVE_REMOVABLE || type == DRIVE_RAMDISK;
}

static void print_usage(void)
{
    wprintf(L"Microsoft ScanDisk compatibility utility for Water\n\n");
    wprintf(L"Usage: scandisk [drive:] [/all] [/autofix] [/checkonly] [/custom]\n");
    wprintf(L"                [/nosave] [/nosummary] [/surface] [/mono]\n\n");
    wprintf(L"  /all        Check all local drives.\n");
    wprintf(L"  /autofix    Request automatic repair. Water currently scans read-only.\n");
    wprintf(L"  /checkonly  Check without making repairs.\n");
    wprintf(L"  /custom     Use custom ScanDisk policy. Repair policy is not implemented yet.\n");
    wprintf(L"  /nosave     With /autofix, discard lost clusters instead of saving them.\n");
    wprintf(L"  /nosummary  Do not stop for a summary.\n");
    wprintf(L"  /surface    Read every sector after structural checks.\n");
    wprintf(L"  /mono       Use monochrome output (accepted for compatibility).\n");
}

static BOOL parse_switch(const WCHAR *arg, struct scan_options *options)
{
    const WCHAR *name = arg + 1;

    if (!lstrcmpiW(name, L"all")) options->all = TRUE;
    else if (!lstrcmpiW(name, L"autofix")) options->autofix = TRUE;
    else if (!lstrcmpiW(name, L"checkonly")) options->checkonly = TRUE;
    else if (!lstrcmpiW(name, L"custom")) options->custom = TRUE;
    else if (!lstrcmpiW(name, L"nosave")) options->nosave = TRUE;
    else if (!lstrcmpiW(name, L"nosummary")) options->nosummary = TRUE;
    else if (!lstrcmpiW(name, L"surface")) options->surface = TRUE;
    else if (!lstrcmpiW(name, L"mono")) options->mono = TRUE;
    else if (!lstrcmpW(name, L"?"))
        options->help = TRUE;
    else return FALSE;

    return TRUE;
}

static BOOL is_fat_fsname(const WCHAR *name)
{
    return !lstrcmpiW(name, L"FAT") || !lstrcmpiW(name, L"FAT12") ||
           !lstrcmpiW(name, L"FAT16") || !lstrcmpiW(name, L"FAT32");
}

static BOOL boot_has_fat_label(const BYTE *boot)
{
    return !memcmp(boot + 0x36, "FAT", 3) || !memcmp(boot + 0x52, "FAT", 3);
}

static BOOL parse_drive(const WCHAR *arg, WCHAR *drive)
{
    WCHAR c;

    if (!arg[0] || arg[1] != L':') return FALSE;
    if (arg[2] && !(arg[2] == L'\\' && !arg[3])) return FALSE;

    c = arg[0];
    if (c >= L'a' && c <= L'z') c -= L'a' - L'A';
    if (c < L'A' || c > L'Z') return FALSE;

    *drive = c;
    return TRUE;
}

static void check_dirty_state(HANDLE handle, struct scan_result *result)
{
    DWORD flags = 0, returned = 0;

    if (DeviceIoControl(handle, FSCTL_IS_VOLUME_DIRTY, NULL, 0, &flags, sizeof(flags), &returned, NULL))
    {
        wprintf(L"  Volume state: %s\n", (flags & VOLUME_DIRTY_FLAG) ? L"dirty" : L"clean");
        if (flags & VOLUME_DIRTY_FLAG) result->problems++;
        return;
    }

    switch (GetLastError())
    {
    case ERROR_INVALID_FUNCTION:
    case ERROR_NOT_SUPPORTED:
    case ERROR_INVALID_PARAMETER:
        wprintf(L"  Volume state: FSCTL_IS_VOLUME_DIRTY is not supported by this Water path.\n");
        result->incomplete++;
        break;
    default:
        wprintf(L"  Volume state: query failed (error %lu).\n", GetLastError());
        result->incomplete++;
        break;
    }
}

static BOOL decode_fat_layout(const BYTE *boot, struct fat_layout *layout, struct scan_result *result)
{
    DWORD fat16, fat32, total16, total32, overhead;
    ULONGLONG root_bytes;

    ZeroMemory(layout, sizeof(*layout));

    if (boot[510] != 0x55 || boot[511] != 0xaa)
    {
        wprintf(L"  FAT boot sector: missing 55 AA signature.\n");
        result->problems++;
        return FALSE;
    }

    layout->bytes_per_sector = get_u16(boot + 0x0b);
    layout->sectors_per_cluster = boot[0x0d];
    layout->reserved_sectors = get_u16(boot + 0x0e);
    layout->fat_count = boot[0x10];
    layout->root_entries = get_u16(boot + 0x11);
    total16 = get_u16(boot + 0x13);
    fat16 = get_u16(boot + 0x16);
    total32 = get_u32(boot + 0x20);
    fat32 = get_u32(boot + 0x24);

    if (!is_power_of_two(layout->bytes_per_sector) ||
        layout->bytes_per_sector < 512 || layout->bytes_per_sector > 4096)
    {
        wprintf(L"  FAT boot sector: invalid bytes/sector value %lu.\n", layout->bytes_per_sector);
        result->problems++;
        return FALSE;
    }

    if (!is_power_of_two(layout->sectors_per_cluster) || layout->sectors_per_cluster > 128)
    {
        wprintf(L"  FAT boot sector: invalid sectors/cluster value %lu.\n", layout->sectors_per_cluster);
        result->problems++;
        return FALSE;
    }

    if (!layout->reserved_sectors || !layout->fat_count || layout->fat_count > 4)
    {
        wprintf(L"  FAT boot sector: invalid reserved-sector or FAT count.\n");
        result->problems++;
        return FALSE;
    }

    layout->total_sectors = total16 ? total16 : total32;
    layout->sectors_per_fat = fat16 ? fat16 : fat32;
    if (!layout->total_sectors || !layout->sectors_per_fat)
    {
        wprintf(L"  FAT boot sector: missing total-sector or FAT-size field.\n");
        result->problems++;
        return FALSE;
    }

    if (!(boot[0x15] == 0xf0 || boot[0x15] >= 0xf8))
    {
        wprintf(L"  FAT boot sector: suspicious media descriptor %#x.\n", boot[0x15]);
        result->problems++;
    }

    root_bytes = (ULONGLONG)layout->root_entries * 32;
    layout->root_dir_sectors = (DWORD)((root_bytes + layout->bytes_per_sector - 1) /
                                       layout->bytes_per_sector);

    if ((ULONGLONG)layout->reserved_sectors +
        (ULONGLONG)layout->fat_count * layout->sectors_per_fat +
        layout->root_dir_sectors >= layout->total_sectors)
    {
        wprintf(L"  FAT boot sector: metadata consumes the complete volume.\n");
        result->problems++;
        return FALSE;
    }

    overhead = layout->reserved_sectors + layout->root_dir_sectors;
    layout->data_sectors = (ULONGLONG)layout->total_sectors - overhead -
                           (ULONGLONG)layout->fat_count * layout->sectors_per_fat;
    layout->clusters = layout->data_sectors / layout->sectors_per_cluster;

    if (layout->clusters <= FAT12_MAX_CLUSTERS) layout->kind = FAT_KIND_12;
    else if (layout->clusters <= FAT16_MAX_CLUSTERS) layout->kind = FAT_KIND_16;
    else layout->kind = FAT_KIND_32;

    if (layout->kind == FAT_KIND_32)
    {
        layout->ext_flags = get_u16(boot + 0x28);
        layout->fsinfo_sector = get_u16(boot + 0x30);
        layout->backup_boot_sector = get_u16(boot + 0x32);

        if (layout->root_entries || fat16)
        {
            wprintf(L"  FAT32 boot sector: FAT12/16-only fields are nonzero.\n");
            result->problems++;
        }
        if (get_u32(boot + 0x2c) < 2)
        {
            wprintf(L"  FAT32 boot sector: invalid root-directory cluster.\n");
            result->problems++;
        }
    }
    else if (!layout->root_entries || !fat16)
    {
        wprintf(L"  %s boot sector: missing FAT12/16 root-directory fields.\n", fat_name(layout->kind));
        result->problems++;
    }

    wprintf(L"  FAT layout: %s, %lu bytes/sector, %lu sectors/cluster, %llu clusters.\n",
            fat_name(layout->kind), layout->bytes_per_sector, layout->sectors_per_cluster,
            (unsigned long long)layout->clusters);
    return TRUE;
}

static void check_fat32_fsinfo(HANDLE handle, const struct fat_layout *layout,
                               struct scan_result *result)
{
    BYTE *sector;
    ULONGLONG offset;
    DWORD free_count, next_free;

    if (layout->kind != FAT_KIND_32) return;

    if (!layout->fsinfo_sector || layout->fsinfo_sector >= layout->reserved_sectors)
    {
        wprintf(L"  FAT32 FSInfo: sector %lu is outside the reserved area.\n",
                layout->fsinfo_sector);
        result->problems++;
        return;
    }

    sector = HeapAlloc(GetProcessHeap(), 0, layout->bytes_per_sector);
    if (!sector)
    {
        result->incomplete++;
        return;
    }

    offset = (ULONGLONG)layout->fsinfo_sector * layout->bytes_per_sector;
    if (!read_at(handle, offset, sector, layout->bytes_per_sector))
    {
        wprintf(L"  FAT32 FSInfo: cannot read sector %lu (error %lu).\n",
                layout->fsinfo_sector, GetLastError());
        result->problems++;
        HeapFree(GetProcessHeap(), 0, sector);
        return;
    }

    if (get_u32(sector) != FAT32_FSINFO_LEAD_SIG ||
        get_u32(sector + 484) != FAT32_FSINFO_STRUC_SIG ||
        get_u32(sector + 508) != FAT32_FSINFO_TRAIL_SIG)
    {
        wprintf(L"  FAT32 FSInfo: signature mismatch.\n");
        result->problems++;
        HeapFree(GetProcessHeap(), 0, sector);
        return;
    }

    free_count = get_u32(sector + 488);
    next_free = get_u32(sector + 492);

    if (free_count != 0xffffffffu && free_count > layout->clusters)
    {
        wprintf(L"  FAT32 FSInfo: free-cluster count %lu exceeds the volume cluster count.\n",
                free_count);
        result->problems++;
    }
    if (next_free != 0xffffffffu && (next_free < 2 || next_free >= layout->clusters + 2))
    {
        wprintf(L"  FAT32 FSInfo: next-free hint %lu is outside the data-cluster range.\n",
                next_free);
        result->problems++;
    }

    wprintf(L"  FAT32 FSInfo: signatures valid.\n");
    HeapFree(GetProcessHeap(), 0, sector);
}

static void check_fat32_backup_boot(HANDLE handle, const struct fat_layout *layout,
                                    const BYTE *primary, struct scan_result *result)
{
    BYTE backup[512];
    ULONGLONG offset;

    if (layout->kind != FAT_KIND_32) return;

    if (layout->backup_boot_sector != 6)
    {
        wprintf(L"  FAT32 backup BPB: BPB_BkBootSec is %lu; Microsoft-compatible FAT32 expects sector 6.\n",
                layout->backup_boot_sector);
        result->problems++;
    }

    if (!layout->backup_boot_sector ||
        layout->backup_boot_sector >= layout->reserved_sectors)
    {
        wprintf(L"  FAT32 backup BPB: sector is outside the reserved area.\n");
        result->problems++;
        return;
    }

    offset = (ULONGLONG)layout->backup_boot_sector * layout->bytes_per_sector;
    if (!read_at(handle, offset, backup, sizeof(backup)))
    {
        wprintf(L"  FAT32 backup BPB: cannot read sector %lu (error %lu).\n",
                layout->backup_boot_sector, GetLastError());
        result->problems++;
        return;
    }

    if (backup[510] != 0x55 || backup[511] != 0xaa)
    {
        wprintf(L"  FAT32 backup BPB: missing 55 AA signature.\n");
        result->problems++;
        return;
    }

    /*
     * Compare the BPB and FAT32 extended-BPB fields, not bootstrap code.
     * Offsets 0x0b..0x59 contain the common and FAT32 BPB structures.
     */
    if (memcmp(primary + 0x0b, backup + 0x0b, 0x5a - 0x0b))
    {
        wprintf(L"  FAT32 backup BPB: primary and backup BPB fields differ.\n");
        result->problems++;
        return;
    }

    wprintf(L"  FAT32 backup BPB: primary and backup BPB fields match.\n");
}

static void compare_fat_copies(HANDLE handle, const struct fat_layout *layout,
                               struct scan_result *result)
{
    BYTE *first, *other;
    ULONGLONG fat_bytes, done;
    DWORD chunk, copy;

    if (layout->fat_count < 2) return;

    if (layout->kind == FAT_KIND_32 && (layout->ext_flags & 0x0080))
    {
        wprintf(L"  FAT copies: mirroring disabled; active FAT is %lu.\n",
                layout->ext_flags & 0x000f);
        return;
    }

    chunk = 64 * 1024;
    if (chunk < layout->bytes_per_sector) chunk = layout->bytes_per_sector;
    chunk -= chunk % layout->bytes_per_sector;

    first = HeapAlloc(GetProcessHeap(), 0, chunk);
    other = HeapAlloc(GetProcessHeap(), 0, chunk);
    if (!first || !other)
    {
        HeapFree(GetProcessHeap(), 0, first);
        HeapFree(GetProcessHeap(), 0, other);
        result->incomplete++;
        return;
    }

    fat_bytes = (ULONGLONG)layout->sectors_per_fat * layout->bytes_per_sector;

    for (copy = 1; copy < layout->fat_count; copy++)
    {
        BOOL mismatch = FALSE;

        for (done = 0; done < fat_bytes; done += chunk)
        {
            DWORD size = (DWORD)((fat_bytes - done < chunk) ? fat_bytes - done : chunk);
            ULONGLONG first_offset =
                (ULONGLONG)layout->reserved_sectors * layout->bytes_per_sector + done;
            ULONGLONG copy_offset =
                ((ULONGLONG)layout->reserved_sectors +
                 (ULONGLONG)copy * layout->sectors_per_fat) * layout->bytes_per_sector + done;

            if (!read_at(handle, first_offset, first, size) ||
                !read_at(handle, copy_offset, other, size))
            {
                wprintf(L"  FAT copies: read failed while comparing FAT %lu (error %lu).\n",
                        copy + 1, GetLastError());
                result->incomplete++;
                mismatch = TRUE;
                break;
            }

            if (memcmp(first, other, size))
            {
                wprintf(L"  FAT copies: FAT 1 and FAT %lu differ.\n", copy + 1);
                result->problems++;
                mismatch = TRUE;
                break;
            }
        }

        if (!mismatch)
            wprintf(L"  FAT copies: FAT 1 and FAT %lu match.\n", copy + 1);
    }

    HeapFree(GetProcessHeap(), 0, other);
    HeapFree(GetProcessHeap(), 0, first);
}

static void surface_scan(HANDLE handle, const struct fat_layout *layout,
                         struct scan_result *result)
{
    BYTE *buffer;
    ULONGLONG total_bytes, offset;
    DWORD chunk, bad_sectors = 0;

    total_bytes = (ULONGLONG)layout->total_sectors * layout->bytes_per_sector;
    chunk = 1024 * 1024;
    chunk -= chunk % layout->bytes_per_sector;

    buffer = HeapAlloc(GetProcessHeap(), 0, chunk);
    if (!buffer)
    {
        result->incomplete++;
        return;
    }

    wprintf(L"  Surface scan: reading %lu sectors.\n", layout->total_sectors);

    for (offset = 0; offset < total_bytes; offset += chunk)
    {
        DWORD size = (DWORD)((total_bytes - offset < chunk) ? total_bytes - offset : chunk);

        if (read_at(handle, offset, buffer, size)) continue;

        {
            ULONGLONG sector_offset;
            for (sector_offset = offset; sector_offset < offset + size;
                 sector_offset += layout->bytes_per_sector)
            {
                if (!read_at(handle, sector_offset, buffer, layout->bytes_per_sector))
                    bad_sectors++;
            }
        }
    }

    if (bad_sectors)
    {
        wprintf(L"  Surface scan: %lu unreadable sector(s).\n", bad_sectors);
        result->problems += bad_sectors;
    }
    else wprintf(L"  Surface scan: no read errors detected.\n");

    HeapFree(GetProcessHeap(), 0, buffer);
}

static struct scan_result scan_drive(WCHAR drive, const struct scan_options *options)
{
    struct scan_result result = {0, 0};
    struct fat_layout layout;
    WCHAR root[] = L"A:\\";
    WCHAR device[] = L"\\\\.\\A:";
    WCHAR fsname[32] = L"", label[MAX_PATH] = L"";
    DWORD serial, max_component, fsflags;
    DWORD sectors_per_cluster, bytes_per_sector, free_clusters, total_clusters;
    BYTE boot[512];
    HANDLE handle;
    UINT type;
    BOOL have_fat = FALSE, fs_known = FALSE, api_fat = FALSE;

    root[0] = drive;
    device[4] = drive;

    wprintf(L"\nDrive %c:\n", drive);
    type = GetDriveTypeW(root);
    if (!is_local_drive(type))
    {
        wprintf(L"  ScanDisk only checks local writable-style volumes; drive type %u is unsupported.\n", type);
        result.incomplete++;
        return result;
    }

    if (GetVolumeInformationW(root, label, ARRAY_SIZE(label), &serial, &max_component,
                              &fsflags, fsname, ARRAY_SIZE(fsname)))
    {
        fs_known = fsname[0] != 0;
        api_fat = fs_known && is_fat_fsname(fsname);
        wprintf(L"  Volume: %s  File system: %s  Serial: %08lx\n",
                label[0] ? label : L"(no label)", fs_known ? fsname : L"(unknown)", serial);
    }
    else
        wprintf(L"  Volume information unavailable (error %lu).\n", GetLastError());

    if (GetDiskFreeSpaceW(root, &sectors_per_cluster, &bytes_per_sector,
                          &free_clusters, &total_clusters))
        wprintf(L"  Logical geometry: %lu bytes/sector, %lu sectors/cluster, %lu/%lu clusters free.\n",
                bytes_per_sector, sectors_per_cluster, free_clusters, total_clusters);

    handle = CreateFileW(device, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL, OPEN_EXISTING, 0, NULL);
    if (handle == INVALID_HANDLE_VALUE)
    {
        wprintf(L"  Raw volume access unavailable (error %lu); FAT structural checks cannot run.\n",
                GetLastError());
        result.incomplete++;
        goto done;
    }

    check_dirty_state(handle, &result);

    if (read_at(handle, 0, boot, sizeof(boot)))
    {
        /*
         * Do not interpret an NTFS/exFAT/etc. BPB as damaged FAT. If the
         * filesystem API is unavailable, the legacy FAT type strings are only
         * a detection hint; the cluster-count calculation remains authoritative
         * once we decide to inspect the FAT layout.
         */
        if (api_fat || (!fs_known && boot_has_fat_label(boot)))
            have_fat = decode_fat_layout(boot, &layout, &result);
        else if (fs_known)
            wprintf(L"  Structural FAT scan skipped for %s.\n", fsname);
        else
        {
            wprintf(L"  No FAT identity could be established from the API or boot sector.\n");
            result.incomplete++;
        }
    }
    else
    {
        wprintf(L"  Boot sector read failed (error %lu).\n", GetLastError());
        result.incomplete++;
    }

    if (have_fat)
    {
        if (fsname[0])
        {
            if (layout.kind == FAT_KIND_32 && lstrcmpiW(fsname, L"FAT32"))
            {
                wprintf(L"  File-system identity mismatch: API reports %s, BPB classifies FAT32.\n", fsname);
                result.problems++;
            }
            else if ((layout.kind == FAT_KIND_12 || layout.kind == FAT_KIND_16) &&
                     lstrcmpiW(fsname, L"FAT") && lstrcmpiW(fsname, L"FAT12") &&
                     lstrcmpiW(fsname, L"FAT16"))
            {
                wprintf(L"  File-system identity mismatch: API reports %s, BPB classifies %s.\n",
                        fsname, fat_name(layout.kind));
                result.problems++;
            }
        }

        check_fat32_fsinfo(handle, &layout, &result);
        check_fat32_backup_boot(handle, &layout, boot, &result);
        compare_fat_copies(handle, &layout, &result);
        if (options->surface) surface_scan(handle, &layout, &result);
    }

    CloseHandle(handle);

done:
    if (options->autofix || options->custom)
    {
        wprintf(L"  Repair was requested, but Water ScanDisk is intentionally read-only until "
                L"the raw write/locking path is implemented and validated.\n");
        result.incomplete++;
    }

    return result;
}

int __cdecl wmain(int argc, WCHAR *argv[])
{
    struct scan_options options = {0};
    BOOL drives[26] = {0};
    struct scan_result total = {0, 0};
    DWORD mask;
    int i;

    TRACE("Command line:");
    for (i = 0; i < argc; i++) TRACE(" %s", debugstr_w(argv[i]));
    TRACE("\n");

    for (i = 1; i < argc; i++)
    {
        WCHAR drive;

        if ((argv[i][0] == L'/' || argv[i][0] == L'-') && argv[i][1])
        {
            if (!parse_switch(argv[i], &options))
            {
                wprintf(L"Unknown ScanDisk option: %s\n", argv[i]);
                print_usage();
                return 2;
            }
        }
        else if (parse_drive(argv[i], &drive))
            drives[drive - L'A'] = TRUE;
        else
        {
            wprintf(L"Invalid drive or option: %s\n", argv[i]);
            print_usage();
            return 2;
        }
    }

    if (options.help)
    {
        print_usage();
        return 0;
    }

    if (options.autofix && options.checkonly)
    {
        wprintf(L"/autofix and /checkonly cannot be used together.\n");
        return 2;
    }

    if (options.nosave && !options.autofix)
        wprintf(L"Warning: /nosave only has meaning with /autofix.\n");

    if (options.all)
    {
        mask = GetLogicalDrives();
        for (i = 0; i < 26; i++)
        {
            WCHAR root[] = L"A:\\";
            root[0] = L'A' + i;
            if ((mask & (1u << i)) && is_local_drive(GetDriveTypeW(root)))
                drives[i] = TRUE;
        }
    }

    for (i = 0; i < 26; i++) if (drives[i]) break;
    if (i == 26)
    {
        WCHAR current[MAX_PATH], drive;

        if (options.all)
        {
            wprintf(L"No local drives were found.\n");
            return 3;
        }

        if (!GetCurrentDirectoryW(ARRAY_SIZE(current), current) || current[1] != L':')
        {
            wprintf(L"Cannot determine the current drive.\n");
            return 2;
        }

        drive = current[0];
        if (drive >= L'a' && drive <= L'z') drive -= L'a' - L'A';
        if (drive < L'A' || drive > L'Z')
        {
            wprintf(L"Cannot determine the current drive.\n");
            return 2;
        }
        drives[drive - L'A'] = TRUE;
    }

    for (i = 0; i < 26; i++)
    {
        struct scan_result one;

        if (!drives[i]) continue;
        one = scan_drive(L'A' + i, &options);
        total.problems += one.problems;
        total.incomplete += one.incomplete;
    }

    if (!options.nosummary)
    {
        wprintf(L"\nScanDisk summary: %u problem(s), %u incomplete check(s).\n",
                total.problems, total.incomplete);
    }

    if (total.problems) return 1;
    if (total.incomplete) return 3;
    return 0;
}
