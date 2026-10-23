# Windows 3.1 386 Enhanced Control Panel — CPWIN386.CPL

Status: HISTORICAL FILE OWNERSHIP AND CONFIGURATION VERIFIED FROM
CONTEMPORARY MICROSOFT KB/RESOURCE KIT; WATER CODE INSPECTED;
NO ORIGINAL CPWIN386.CPL BINARY OR INSTALL MEDIA EXTRACTED;
NO WIN16 APPLET, SWAP MANAGER, OR 32-BIT DISK/FILE I/O IMPLEMENTED.
Reviewed: 2026-10-23.

This is the Windows 3.x companion to
`windows-application-compatibility.md` and `winini-parsing.md`.
Do not conflate the Win3.1 OS control-panel extension with an independent
third-party application, NT System Properties, modern `uxtheme`, or
Windows 95/98's later storage stacks. No-reset: retain existing Win386,
INT13, IOS, IFSMGR, 16-bit ABI, and profile-parser boundaries.

## Module identity and distribution evidence

| Historical name | Role | Distribution / ABI | Evidence |
| --- | --- | --- | --- |
| `CONTROL.EXE` | Windows 3.1 Win16 Control Panel host | OS application / NE, not Water's modern `control.exe` executable | 1992 Windows 3.1 Resource Kit; SDK Chapter 15 |
| `CPWIN386.CPL` | **386 Enhanced** Control Panel extension (virtual memory, disk access, DOS VM settings) | Win3.1 OS-supplied CPL / Win16 module, not `MAIN.CPL` | Microsoft Q86023 and Windows 3.1 Resource Kit |
| `MAIN.CPL` | Other standard Win3.1 applets | Separate Win16 OS CPL | Microsoft Q86023 |
| `CPWIN386.CP_` | Compressed setup artifact for `CPWIN386.CPL` | Win3.1 disk 1 (3.5-inch or 5.25-inch); WfW 3.11 disk 2 (both) | Microsoft Q86023 |
| `SYSTEM.INI` | OS/driver/386 enhanced-mode configuration | Installed guest configuration, possibly customized | Windows 3.1 Resource Kit Q83435/Q83436 |
| `386SPART.PAR` | Permanent swap-file artifact | Machine-specific virtual-memory data, not an installable applet | WfW 3.11 troubleshooting; binary not examined |

Q86023 lists a **104,816-byte expanded** Win3.1 `CPWIN386.CPL` and
a **48,841-byte compressed** `CPWIN386.CP_`. Those are archived
distribution-inventory figures, NOT a hash or verification of any
Water-installed binary. Do not assume WfW 3.11's file is identical.
The 1992 SDK confirms a Win16 Control Panel applet resides in a
DLL, discovered and invoked by `CONTROL.EXE`; original exports and
ordinals should be verified against a lawful original NE image.
Do not silently replace this with Win32 `CPlApplet`.

## Historical UI and three independent settings

Path for Windows 3.1 and WfW 3.11: **Control Panel → 386 Enhanced →
Virtual Memory → Change**. Exact conditional controls depend on
release, enhanced-mode status, compatible drivers and guest hardware.

1. **Paging / swap-file configuration**: demand paging, temporary
   (`WIN386.SWP`) versus permanent (`386SPART.PAR`) swap,
   selected drive and size in KB, free space, and maximum/recommended
   size. Resource Kit Q83436 documents distinct `[386Enh]` keys:
   `Paging`, `PagingDrive`, `PagingFile`,
   `PermSwapDOSDrive`, `PermSwapSizeK`,
   `MaxPagingFileSize`. The temporary and permanent paths are NOT
   interchangeable. Verify exact application behavior from original
   setup and original UI before implementing writes.
2. **32-bit DISK access**: Windows 3.1's FastDisk/WDCTRL-compatible
   block-level disk access. `[386Enh]` key
   `32BitDiskAccess=On/Off` expresses the requested configuration,
   not proof of an active driver. Q83435 says the checkbox is not
   displayed if the key is absent when the hard disk is unsupported;
   the original UI and 32-bit disk path also depend on compatible
   controllers/virtual drivers. The change needs a Windows restart.
   Do not enable a real disk controller by simply setting this key.
3. **32-bit FILE access**: a **different** WfW 3.11-specific
   VFAT.386/VCACHE.386 cached filesystem access mechanism, with a
   user-configurable cache size and per-drive active status.
   Do not show the WfW 3.11 control in plain Windows 3.1 or WfW 3.1.
   Microsoft Q109510 identifies IFSHLP.SYS/VSHARE.386 and certain
   real-mode redirectors as actual compatibility prerequisites;
   Q108079 records a 24,576 KB GUI maximum for the WfW 3.11 cache.
   Its `[VCache]` `MinFileCache`/`MaxFileCache` configuration is
   independent of the `[386Enh]` disk-access toggle. The presence
   of `32BitDiskAccess=On` does not establish that 32-bit file
   access is available or enabled.

**OS/version boundary:** Windows 3.0 had 386-enhanced-mode controls,
but this ledger has not verified its `CPWIN386.CPL` packaging or
identical dialog layout. Windows 3.1/WfW 3.1 disk-access behavior,
WfW 3.11 VFAT file-access behavior, and Win95/Win98 I/O Supervisor
must remain distinct. Never import NT paging-file UI or NT kernel
storage semantics into Win3.1 because the names sound alike.

## Read-only status versus requested settings

A correct UI must distinguish at least these independent facts:

- Key present? (checkbox eligibility on the documented Win3.1 path)
- Setting requested? (value read from the *guest* `SYSTEM.INI`)
- Hardware/driver eligible? (FastDisk, WDCTRL, VFAT, VCACHE,
  IFSHLP, controller, per-drive restrictions)
- Operational in this session? (confirmed VMM/drive status)
- Requested change pending restart? (persisted settings cannot be
  reported as active immediately)

Display **unsupported/unknown** rather than guessing active disk
or paging mode. No write should occur if the backing emulated
controller, VMM state, guest filesystem, swap allocation, or rollback
mechanism is missing. A file-backed `SYSTEM.INI` is not a hardware
capability oracle.

## Current Water code audit

| Subsystem / source | Present behavior | Missing prerequisite |
| --- | --- | --- |
| `programs/control/control.c` | Win32 executable delegating to `Control_RunDLLW` | Original Win3.1 Win16 CONTROL.EXE host and Win16 applet lifecycle |
| `dlls/shell32/control.c` | Modern Win32 `CPlApplet` discovery | Does not establish a Win16 NE CPL registration path |
| `dlls/cpwin386.cpl16` | No such directory in audited `dlls/` tree | Native Win16 CPL exports, resource/dialog ABI, personality gate |
| `programs/win386/win386.c` | Enhanced-mode session identity, WfW 3.1/3.11 product flags | Actual pager state, disk capability enumeration, pending restart state |
| `dlls/krnl386.exe16/win386.c` | Queries validated WIN386 session metadata | Query for supported/active paging and disk access |
| `dlls/vmm.vxd/vmm.c` | Partial virtual memory services via VxDCall | Full Win3.x page swapping, permanent swap allocation, FastDisk |
| `dlls/krnl386.exe16/file.c` and `dlls/kernel32/profile.c` | Existing Win16 profile wrappers / shared INI parser | No audited CPL owner/writer for `[386Enh]` keys |
| `programs/ntio.sys/int13.c` and `dlls/int13.vxd` | Separate DOS and **Win95** INT13 compatibility work | Not interchangeable with Win3.1 WDCTRL/FastDisk |
| `dlls/ifsmgr.vxd`, `dlls/ios.vxd` | Separate 9x VxD layers | Not evidence of functioning WfW 3.11 VFAT/VCACHE |

No `cpwin386.cpl16`/original Win3.1 CPL build target found during
the October 23 repository directory audit. This is **not** a claim
that Water has no generic paging or disk APIs anywhere else.

## Minimal implementation path with failure guards

1. **Evidence first**: extract `CPWIN386.CP_` from original media
   (Win3.1 and WfW 3.11 separately); record expanded NE headers,
   imports, exports/ordinals, CPL entrypoints, dialog resources,
   file hashes and setup provenance. Test loading the authentic
   CPL through a 16-bit Control Panel host before replacing it.
2. **Win16 applet/host ABI**: implement host `CPlApplet` interaction
   and discovery according to the Windows 3.1 SDK. If a source
   implementation is necessary, put a 16-bit NE personality behind
   an explicit build/runtime gate rather than masquerading as
   `dlls/shell32`/modern Win32 `control.exe`.
3. **Read-only first**: show the real guest `[386Enh]` settings and
   statuses only where independently available. Do not allow a checkbox
   merely because the config contains `32BitDiskAccess=On`.
   Test disabled, omitted, malformed, and oversized values.
4. **Pager backend**: only after VMM paging, active swap mapping,
   safe free-space sizing, file locking, and temporary/permanent
   lifecycle work. Never allocate a 386SPART.PAR by an unchecked
   applet action, and never touch physical/raw host disks.
5. **Disk backend**: require a compatible emulated Win3.1 FastDisk
   path and per-drive status. Unsupported hardware must fail
   closed. Keep original DOS BIOS 13h and Win95 IOS/INT13.VXD apart.
6. **WfW 3.11-only file backend**: verify original VFAT/VCACHE
   behavior, IFSHLP, redirector conflicts, and cache-size limits
   before enabling/advertising any file-access option.
7. **Transactional configuration**: guest-only backups, atomic
   changes where possible, correct `SYSTEM.INI`/section/key case
   handling, a restart requirement, and rollback if activation
   fails. Never quietly change an already running VM's I/O mode.

### Proposed compatibility matrix

| Test | Win3.1 standard | Win3.1 enhanced | WfW 3.11 enhanced |
| --- | --- | --- | --- |
| 386 Enhanced entry availability | Absent/inactive; original behavior to confirm | Present on a supported setup | Present on a supported setup |
| Virtual-memory state | Not an active 386 pager | Temporary/permanent/off variants | Temporary/permanent/off variants |
| `32BitDiskAccess` key absent | No fabricated active 32-bit disk | UI checkbox absent per Q83435 | Compare genuine WfW behavior |
| `32BitDiskAccess=On`, incompatible controller | Never report active | Report unsupported/fallback | Report unsupported/fallback |
| 32-bit FILE access | Absent | Absent | Conditional; dependent on VFAT/VCACHE |
| Change settings without restart | No false immediate activation | Pending, not active | Pending, not active |
| Unsupported host/raw disk | Must never mutate | Must never mutate | Must never mutate |

These are proposed expected conditions drawn from period references.
Any case without a real reference installation is marked **NOT RUN**
until verified; no actual emulator compatibility is claimed.

## Sources

- Microsoft KB **Q86023**, *Control Panel .CPL Files and Icon
  Installation*: files, shipped compressed/expanded sizes, disc numbers:
  https://jeffpar.github.io/kbarchive/kb/086/Q86023/
- Microsoft Windows 3.1 Resource Kit (1992), Chapter 3 Windows files:
  https://www.bitsavers.org/pdf/microsoft/resource_kits/0030-31645_Windows_3.1_Resource_Kit_199202.pdf
- Microsoft Windows 3.1 SDK (1992), Chapter 15 Control Panel apps:
  https://bitsavers.trailing-edge.com/pdf/microsoft/windows_3.1/Microsoft_Windows_3.1_SDK_1992/PC28915-0492_Programmers_Reference_Volume_1_199204.pdf
- Microsoft KB **Q83435** and **Q83436**, Win3.1 `SYSTEM.INI` `[386Enh]`:
  https://jeffpar.github.io/kbarchive/kb/083/Q83435/
  https://jeffpar.github.io/kbarchive/kb/083/Q83436/
- Microsoft KB **Q109500**, WfW 3.11 `SYSTEM.INI`:
  https://jeffpar.github.io/kbarchive/kb/109/Q109500/
- Microsoft WfW 3.11 User's Guide and Resource Kit (1993):
  https://bitsavers.org/pdf/microsoft/windows_3.1/Windows_for_Workgroups_3.11_Users_Guide_1993.pdf
  https://ftpmirror.your.org/pub/misc/bitsavers/pdf/microsoft/resource_kits/0030-54303_Windows_for_Workgroups_3.11_Resource_Kit_1993.pdf
- Microsoft KB **Q109510** (IFSHLP, redirectors and file access):
  https://jeffpar.github.io/kbarchive/kb/109/Q109510/
- Microsoft KB **Q108079** (WfW 3.11 cache limit and `[VCache]`):
  https://jeffpar.github.io/kbarchive/kb/108/Q108079/
