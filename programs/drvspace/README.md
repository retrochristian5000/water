# DriveSpace volume eligibility (read-only foundation)

## Host prefix versus guest sectors

The default prefix C: drive is a directory, often on the user's home
APFS volume, **not** a separate FAT16 partition. 8.3 filename
compatibility, DOS file attributes, and synthesized free-space data
do not establish actual FAT16 allocation tables or guest sectors.

Until a guest-owned disk-image backend is explicitly mounted,
DOS/Win9x KRNL386 and NTVDM DEM raw sector writes **fail closed** for
every drive and every fake-success setting. INT 26h now asks for genuine
write results; BIOS INT 13h already does. Normal prefix file access and
the preexisting raw-read path remain unchanged. This protection is
specific to these DOS-sector entry points, not all Windows disk APIs.

Do not translate a prefix symlink or \\.\X: device name into a host
partition write. When introducing virtual disks, verify backend identity,
sector bounds, exclusive guest storage ownership, and a shared
file/sector view before allowing any sector write.

Regression: python3 scripts/tests/test_guest_sector_write_safety.py

Windows 98 release notes and PROGRAMS.TXT state that DriveSpace 3 can
**detect** FAT32 but does **not compress** FAT32 volumes. That limitation
is independent of Windows 95 OSR2 / Windows 98 / Me being able to mount
FAT32 through their DOS 7.1/8.0 and Windows disk stacks. It applies to
DriveSpace's compression operation, not to ScanDisk or normal FAT32 I/O.

This directory presently contains a *policy preflight*, not DRVSPACE.EXE:
DRIVESPACE_ClassifyTarget checks a zero-based guest DOS drive using the
existing IO.SYS read-only verified BPB readers.

- A recognized FAT32 volume returns FAT32_REJECTED. No DriveSpace
  compression/creation/conversion path may turn that into permission.
- A recognized FAT12/FAT16 volume returns FAT12_16_CANDIDATE. It is only
  a candidate; it does not establish CVF or host-drive eligibility,
  enough free space, integrity, writable access or boot safety.
- Invalid indices, inaccessible raw devices, host-only mapped paths,
  unknown or damaged filesystem geometry fail closed as UNKNOWN.
- FAT32 detection comes first. No fallback to FAT16 based on host free
  space, filenames, filesystem labels or Win16 version reporting.
- This preflight is **not** an active runtime rejection yet: the repo has
  no implemented DriveSpace compressor or DRVSPACE.EXE entry point.
  A future implementation must use this policy, revalidate against the
  actual guest-volume identity before writing, and make FAT32 rejection
  a mandatory condition for every compression-capable operation.
- A DriveSpace compressed-volume file (DRVSPACE.000 etc.) and its host
  volume are distinct. This preflight does not parse or mount CVFs.

Regression (compiles the actual policy file with stubbed read-only BPB
providers; no real disks are opened or written):

    python3 scripts/tests/test_drvspace_fat32.py

Historical sources:
- Windows 98 PROGRAMS.TXT:
  https://www.betaarchive.com/wiki/index.php/Windows_98/4.10.1546/programs.txt
- Windows 98 GENERAL.TXT:
  https://www.betaarchive.com/wiki/index.php?title=Windows_98%2F4.10.1546%2Fgeneral.txt
- Microsoft KB Q154997:
  https://helparchive.huntertur.net/document/43322
