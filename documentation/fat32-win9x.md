# Windows 9x FAT32 geometry research and implementation ledger

Status: SOURCE-AUDITED / READ-ONLY FAT32 BPB DECODER ADDED /
RAW-SECTOR EMULATION NOT IMPLEMENTED (2026-10-09).

## Boundaries

- Water itself does NOT need to reside on FAT32. Its normal Win32/host
  file system translation already handles mapped host paths on APFS,
  ext4, NTFS, etc., through NTDLL and kernelbase.
- The DOS 7.1 (Windows 95 OSR2, Windows 98 FE) guest needs a separate
  FAT32 view for low-level DOS disk services. Windows 95 retail / DOS
  7.00 and the NT WOW family must not be silently upgraded to DOS 7.1.
- Kernelbase volume information already recognizes FAT32 mounts.
  DOS INT 21h / 73xx includes partial FAT32 extended services, but
  its extended DPB previously preferred classic FAT12/FAT16 BPBs
  and otherwise synthesized FAT-like fields from host statistics.
- IO.SYS is the proper owner of boot-sector geometry. Its current
  implementation is included in KRNL386 by a temporary source bridge
  until a real IO.SYS boot path is implemented.

## Implementation

In programs/io.sys/config.c and io_sys.h:

- The existing read-only volume boot-sector reader is shared between
  FAT12/16 and FAT32 BPB decoding.
- FAT32 is decoded from BPB fields rather than trusting the optional
  filesystem name string. The decoder validates sector and cluster
  size, FAT count and capacity, FAT32-specific zero legacy fields,
  cluster counts, root-cluster range, FAT mirroring flags, version,
  FSInfo and backup-sector positions.
- Only an accessible physical FAT32 volume with a valid boot sector
  provides authentic geometry. On an unrelated host filesystem or
  inaccessible raw device, parsing returns FALSE and old host-backed
  file access/fallback remains in effect. No disk writes are added.

In programs/ntdos.sys/int21.c:

- Extended DOS drive-parameter blocks now use verified FAT32 BPB
  geometry for their 32-bit FAT size, data start and cluster count,
  root cluster, FSInfo/backup sectors, and mirroring flags.
- Legacy 16-bit-only FAT fields are not fabricated from 32-bit FAT32
  values. FAT32 free-cluster information is marked unknown until
  real FSInfo/allocator state is read.
- FAT12/16 logic remains separate. The existing behavior for
  non-FAT host-backed drive mappings is retained.

## Regression and limitations

Run this test (uses synthetic boot sectors, never accesses real disks):

    python3 scripts/tests/test_iosys_fat32.py

It compiles the actual IO.SYS decoder with minimal portable C stubs and
tests valid geometry, invalid sizes, FAT12/16 confusion, insufficient
FAT capacity, invalid root clusters, flags, and FSInfo pointers.

This is not yet an image-backed FAT32 filesystem, VFAT implementation,
allocation-table writer, disk format tool, or a claim of successful
Win98 FE boot-time disk compatibility.

Further milestones:
1. Compare extended DPB bytes with an authentic DOS 7.1 FAT32 disk image.
2. Audit INT 21h / 7303h geometry and old GetDiskFreeSpace 2GB limits.
3. Keep virtual LBA-sector mappings separate from host file paths.
4. If required, build an explicit, isolated image-backed FAT32 device
   with mount metadata, sector translation, directory updates, FSInfo
   accounting, and safe write isolation.
5. Verify guest and host storage identities independently on APFS, ext4,
   FAT32, NTFS and network drives.

## Source references

- Microsoft's FastFAT sample, FAT32 packed/unpacked BPB definitions:
  https://github.com/Microsoft/Windows-driver-samples/blob/main/filesys/fastfat/fat.h
- BPB offsets in libfat (devkitPro):
  https://github.com/devkitPro/libfat/blob/master/source/partition.c
- Water: programs/io.sys/config.c, programs/io.sys/io_sys.h,
  programs/ntdos.sys/int21.c, dlls/kernelbase/volume.c,
  dlls/ntdll/unix/file.c.
