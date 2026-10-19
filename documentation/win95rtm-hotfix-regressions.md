# Windows 95 RTM hotfix regressions: IFSMGR and installer provenance

Status: PRIMARY MICROSOFT BULLETIN / KB ARCHIVE SOURCE-CHECKED, WATER SOURCE
AUDITED, NO AUTHENTIC PATCH EXECUTABLE OR GUEST FILES INSPECTED, NO FIX
IMPLEMENTED IN A GUEST FILESYSTEM. Last review: 2026-10-19.

This ledger extends WHP's existing Windows 95 RTM work. It does not replace
the existing `win95rtm-add-new-hardware.md`, KERNEL32 ordinal ledger,
`programs/io.sys` boot policy, or NDIS/audio/display ABI documentation.
Hotfixes provide **before/after behavior** and file-version evidence;
their release date does NOT move a Windows 95 RTM interface into a
Windows 98 or Windows NT namespace.

## Hotfix timeline and source strength

| Evidence | Original target | Fixed component and version | Size (bytes) | Status |
| --- | --- | --- | ---: | --- |
| Original release baseline | Windows 95 RTM (4.00.950) | IFSMGR.VXD version and hash NOT captured here | unknown | separate baseline; do not infer binary identity |
| Q138250, September 1995 | Windows 95 retail | IFSMGR.VXD **4.00.951**, dated 1995-09-19 | 165053 | contemporary KB archived; original image not inspected |
| Q256015 / MS00-017, March 2000 | Windows 95 retail | IFSMGR.VXD **4.00.954**, dated 2000-03-08 | 165057 | Microsoft patch ledger; binary not inspected |
| Q256015 | Windows 95 OSR2 | IFSMGR.VXD **4.00.1116**, dated 2000-03-06 | 185922 | distinct OSR2 branch |
| Q256015 | Windows 98 | IFSMGR.VXD **4.10.2001**, dated 2000-03-06 | 185926 | distinct Win98 branch |
| Q256015 | Windows 98 SE | IFSMGR.VXD **4.10.2223**, dated 2000-03-02 | 185926 | distinct Win98 SE branch |

Version numbers and sizes come from the Microsoft KB archives; file hash,
original packaging, actual host filenames and exported VxD services are
**not** established by this table. Do not compare the `4.00.954` IFSMGR
binary with `4.00.1116` merely by filename; neither byte counts nor
relative version numbers establish source lineage across OSR branches.

### Q138250: literal 8.3 filename tilde

The archived KB describes an 8-character FAT 8.3 filename with `~`
in the **eighth** basename position. Earlier IFSMGR parsing treats
subsequent names sharing the same first seven characters as if they
were long-file-name aliases. The reported retail fix is IFSMGR
`4.00.951` (165053 bytes). This is a parser/alias classification
issue; do not merge it with Q256015's reserved device names or assume
that every 8.3 name containing a tilde is an alias.

Candidate pure-parser test pairs (not yet a Water runtime test):
- `ABCDEF1~.TXT`: literal 8-character 8.3 basename with terminal tilde.
- `ABCDEF1A.TXT`: a separate literal 8.3 name with the same first seven
  characters; must not be reclassified solely due to the prior entry.
- A real long filename with a generated `~1` short alias is a distinct
  case: preserve the actual directory-entry/long-name metadata.
No test should create files or mutate FAT disk images on a real machine.

### Q256015 / MS00-017 (CVE-2000-0168): reserved device components

Microsoft's March 16, 2000 bulletin and Q256015 establish that paths
with **multiple consecutive terminal DOS device-name components** can
trigger an illegal resource access / fatal exception `0E` in filesystem
drivers. Microsoft's description distinguishes a single reserved DOS
device name (already recognized) from a path with multiple such names.
Sources name VFAT, CDFS, VREDIR, NWREDIR, UDF, VDEF and optional
third-party filesystem/redirector drivers as possible fault sites.
Therefore tests must not assume this is a VFAT-only fault.

Candidate **in-memory, non-I/O** classification cases:

| Case | Intended check | Caution |
| --- | --- | --- |
| Single `NUL` device component | Retain existing DOS-device handling | Do not globally ban valid DOS device opens |
| Ordinary `C:\\DATA\\NOTES.TXT` | Ordinary filename | Avoid overrestrictive path guard |
| `C:\\NUL\\AUX` | Detect multiple reserved components in a path | Never open on original unpatched 9x |
| `C:\\CON\\PRN` | Detect same issue independent of particular names | No browser/mail-triggered test in real 9x |
| Mixed-case `C:\\nUl\\aUx` | Case folding must be deliberate | Do not assume Unicode NT normalizer matches DOS 7 |
| Prefix and suffix names `NULL`, `CONSOLE`, `COM10` | Negative controls | Do not match device prefixes blindly |
| `COM1`, `LPT1`, device names with trailing punctuation | Check DOS grammar and normalized components | Exact suffix/extension behavior needs RTM evidence |
| Network `\\\\server\\share\\...` and CD path | Cross-provider path dispatch | File system / redirector selection is distinct |

The fixed binary's exact error code and internal path-normalization
algorithm are **not** published in the bulletin. Do not assert that
Microsoft changed NTDLL, applied a particular filter at a Win32 API,
or returned one universal status code for every driver and path type.

## Source-code audit of Water and implementation boundary

1. `dlls/ifsmgr.vxd/ifsmgr.c` is present. It handles a small
   `DeviceIoControl` interface (IFS_IOCTL_21 and IFS_IOCTL_2F mapped
   to DOS interrupt execution via fixed-width `I386_CONTEXT` packets;
   other controls are unimplemented). It has **no path-component
   parsing**, installable FSD driver dispatch, native 9x VxD service
   table, or implemented `IFSMgr_Ring0_FileIO` entry point.
2. `dlls/ntdll/path.c` provides `RtlIsDosDeviceName_U`,
   `RtlDosPathNameToNtPathName_U_WithStatus` and
   `RtlGetFullPathName_UEx`, based on the NT-style namespace.
   `RtlIsDosDeviceName_U` examines the terminal component. These
   functions serve different Windows-family compatibility needs;
   their behavior is NOT the same as the historical IFSMGR patch.
3. Existing `dlls/ntdll/tests/path.c` tests individual DOS device
   names and NT normalization. It is not a Win95 IFSMGR proof suite.
   Do NOT add original-Win95 live crash-inducing paths to these tests.
4. `dlls/krnl386.exe16/vxd.c` maps certain VxD pseudo services,
   not a complete ring-0 LE IFSMGR/VFAT loader. Injecting a fake
   `IFSMGR` success response would obscure the missing architecture.

**Recommended hardening gate (NOT implemented yet):** once the
authentic 9x IFSMGR/FSD pathname dispatch boundary is identified,
validate every path component and the complete normalized path before
treating a DOS device as a disk or redirector file object; preserve
legitimate device paths and return a defined failure without dereference
or side effects for malformed ones. Separate lexical normalization
from file-system dispatch and do not borrow unverified NT rules.

The 1995 DDK's `IFSMgr_Ring0_FileIO` documentation explicitly states
that **trusted VxD callers must validate their own parameters**; its
ring-0 service does not validate parameters for them. This must be
considered separately from untrusted Win32/browser/mail path entry.

## Q161020: update delivery connects to SetupX (separate axis)

Microsoft's Windows 95 update guide documents self-extracting hotfixes
containing replacement files, `W95upd.inf`, sometimes
`ADVPACK.DLL`/`W95INF16.DLL`/`W95INF32.DLL`, and `QFECHECK`.
It states that the package can use **SETUPX.DLL or ADVPACK.DLL**
to install via INF directives, with version checking so an older
component should not overwrite a newer one. Some later packages
select a target binary based on Windows 95's release variant.
Uninstallation commonly uses a `*_UN.INF` and original media.

This directly reuses the existing WHP source map in
`documentation/win95rtm-add-new-hardware.md` without equating
hotfix installation with Add New Hardware's `DiSelectDevice`:
`DiSelectDevice` selects a candidate hardware driver; hotfix
installers generally use INF sections for file copying/registry
updates and may not call device-selection APIs at all.

Safe future test targets (no installation on a host): inspect an
authentic `256015USA5.EXE` container read-only, record SHA-256 and
package-member names, examine `W95upd.inf` and uninstall INF,
identify per-version IFSMGR selections, and compare with isolated
RTM and OSR2 guest snapshots. Preserve source timestamps and
download provenance. Do not allow an RTM image to be replaced with
the OSR2 image merely because the filename is IFSMGR.VXD.

## AMDK6UPD.EXE: OSR2 high-frequency CPU timing and disk-driver init

**Source state:** Microsoft Q192841, Q234259, Q312108 and an archived
Microsoft Download Center record were read. NO AMDK6UPD.EXE binary,
extracted driver, digital signature, executable hash, update INF, guest
crash, or fixed-vs-original disassembly was inspected. The following
is source evidence and a test plan, NOT a fixed IOS driver.

### Affected systems and mechanism: keep the product split

- Q192841: the early Windows 95 family can report `Device IOS failed
  to initialize` or a Windows Protection Error when an AMD K6-2
  operates at 350 MHz or faster; a later KB revision also mentions
  an AMD Athlon at >=1.1 GHz. Occurrence at 350 MHz is intermittent
  and becomes more likely with higher CPU speed.
- The Microsoft article attributes the IOS fault to a **divide exception
  in IOS.VXD** and other startup divide exceptions to **ESDI_506.PDR**
  or **SCSIPORT.PDR**. Faulty initialisation timing-loop *software*
  is blamed, not a defective processor. Precise divisor expressions,
  CPU-counter instructions and their patched instructions remain unknown.
- Microsoft explicitly provides this fix for **Windows 95 OSR2,
  OSR2.1 and OSR2.5** only. An RTM retail installation may show
  similar errors but **must not load the OSR2 patch's binaries**.
  Windows 98 is described as having resolved the problem.
- `AMDK6UPD.EXE` was itself revised: KB Q234259 finds that the
  original `VFBackup.VXD` regressed some floppy backups. The later
  download center record was published 1999-10-14. File version,
  publication date, archive timestamp and binary hash are distinct.

### Microsoft Q192841 listed OSR2 component inventory

| File | Listed update version | Bytes | Subsystem and role |
| --- | --- | ---: | --- |
| `IOS.VXD` | 4.00.1113 | 68,962 | Protected-mode block I/O supervisor; divide fault reported |
| `ESDI_506.PDR` | 4.00.1118 | 24,426 | Protected-mode IDE disk driver; divide fault reported |
| `SCSIPORT.PDR` | 4.00.1112 | 23,158 | Windows 9x SCSI port; divide fault reported |
| `HSFLOP.PDR` | 4.00.1113 | 25,249 | High-capacity floppy port driver |
| `CDFS.VXD` | 4.00.1112 | 59,153 | CD filesystem driver |
| `DISKTSD.VXD` | 4.00.1112 | 18,809 | Disk type-specific driver |
| `INT13.VXD` | 4.00.1112 | 9,955 | BIOS INT 13h disk interface |
| `VFBackup.VXD` | 4.00.1112 | 18,197 | ORIGINAL update backup VxD; later replaced |
| `NTKERN.VXD` | 4.00.1213 | 111,678 | OSR2.1/2.5 only; original article tabulates 4.00.1213 |

**Do not flatten these into one repair algorithm.** The KB names
divide exceptions only in IOS/ESDI_506/SCSIPORT. For the remaining
files, inclusion in the update does not by itself prove they contained
the same timing-loop defect. In its prose, Q192841 additionally says
`NTKERN.VXD` should display **4.03.1213** in file properties, whereas
its table reports **4.00.1213**. Preserve that unresolved provenance
inconsistency pending an actual file-version resource dump. The
September 18, 1998 package revision did not replace NTKERN on
OSR2.1/2.5, according to the article; verify installer variant.

### Q234259: original versus revised backup VxD

| Package iteration | VFBackup version | Bytes | QFE registration |
| --- | --- | ---: | --- |
| Original AMDK6 update | `4.00.1112` | 18,197 | `...\\Setup\\Updates\\980911N1` |
| Revised AMDK6 update | `4.00.1114` (1999-05-25) | 18,197 | `...\\Setup\\Updates\\990602N1` |

The full registry root is
`HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Setup\\Updates`.
Q234259 states that the revised update replaces the older QFE
registration and that QfeCheck can identify installed patches.
The **identical byte size** of these VFBackup releases is a clear
warning against relying on filename/size as proof of identical code.
`AMDK6_UN.INF` is documented as an uninstall route by a contemporary
download mirror; its own contents and exact rollback semantics are
not independently verified. Do not actually run an archived installer.

### Cross-program audit of the CURRENT Water fork

| Water file or program | Current implementation | Q192841 implication |
| --- | --- | --- |
| `dlls/ios.vxd/ios.c` | IOS VxDCall table has `IOS_GET_VERSION`; remaining register/command/timeout services fail as unsupported. No disk-initialization timing loop is present. | No identical IOS divide instruction or patched code exists to repair. Retain failure rather than fabricate success. |
| `dlls/krnl386.exe16/vxd.c` | Generic VxD service and timer compatibility; timers use host tick functions in places. | NOT proof of IOS's ring-0 clock calibration or CPU-dependent I/O startup loop. |
| `programs/io.sys/config.c` | MSDOS.SYS settings and pure MBR/VBR/FAT decoding; no working IO.SYS boot binary. | Real-mode boot setup is distinct from protected-mode IOS.VXD; do not transplant patch here. |
| `dlls/scsiport.sys` | NT-style Windows driver compatibility module. | **NOT** Windows 95's `SCSIPORT.PDR`; do not port a 9x fix to NT SCSI port by filename resemblance. |
| `dlls/vmm.vxd/vmm.c` | Selected VxD service compatibility; does not form a working original 9x kernel. | Avoid inventing timing APIs or service numbers from hotfix module names. |

**Implementation gate:** once a real 9x IDE/SCSI/IOS initialization
path exists, calibration must use a documented monotonic timebase
and nonzero, range-checked divisors; avoid unsigned wraparound,
unchecked narrowing, busy-loop counts tied to nominal CPU MHz,
and hidden dependency on the host's CPU frequency. This is
**engineering guidance**, not a claim about Microsoft's precise fix.
Preserve disk/device state and report failure on unusable timing data.

Proposed read-only synthetic tests: varying emulated CPU speed
(e.g. 200/300/350/400/500/1100 MHz) while maintaining a consistent
virtual timebase; zero/very-short timer interval; near-overflow
counters; slow/paused clock; restart after failed calibration;
and independent IDE versus SCSI initialization. Check absence of
divide-by-zero, timeout overflow, host I/O and false successful driver
registration. The 350 MHz threshold is a **historical symptom** in
one Microsoft OS release, not a new branch condition to hardcode.

**Independent related issue, NOT part of AMDK6UPD:** Microsoft
Q312108 later described a divide-by-zero in NDIS initialization
at **2.2 GHz and above** on Windows 95/98. That is a separate
network driver/timing issue; do not mistake the two CPU thresholds
or assert that AMDK6UPD fixes NDIS.

### Package acquisition and validation backlog

Identify a genuine OSR2-era `AMDK6UPD.EXE` variant and record source
URL, timestamp, byte length, SHA-256, executable signature if any,
self-extractor `/T:` and `/C` behavior, internal `W95UPD.INF` or other
installer INF, component file resources/hashes, NTKERN conditional
install logic, `VFBackup.VXD` revision, `AMDK6_UN.INF` rollback and
the QFE registry metadata. Inspect the executable **without executing
it** on a host. Confirm all changes with disposable OSR2.0/2.1/2.5
guest snapshots and retain a distinct RTM 4.00.950 control.

### Sources for this extension

- Microsoft KB Q192841 (archived):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/192841
- Microsoft KB Q234259 (original and revised VFBackup/QFE metadata):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/234259
- Archived Microsoft Download Center listing (2012 snapshot; 1999-10-14):
  https://legacyupdate.net/download-center/download/16838/windows-95-windows-95-oem-service-release-2-2.1-2.5-update-for-problems-with-high-speed-amd-k6-2-cpu-amdk6upd.exe
- Microsoft KB Q312108 (distinct NDIS timing fault):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/312108

## WHP gate checklist for other Windows 95 updates

Capture separately: KB number and revision; bulletin/CVE; original
affected versions; exact fixed filenames, four-part versions, file
sizes and hashes; per-release branches; code path and ABI; input
that triggered bug; unchanged/negative controls; actual observed
before/after guest behavior; package/INF installation logic;
rollback/dependencies; build/test status. A newer fix is not
automatically more correct or compatible on RTM.

## Sources

- Microsoft Security Bulletin MS00-017 (2000-03-16):
  https://learn.microsoft.com/en-us/security-updates/securitybulletins/2000/ms00-017
- Microsoft KB Q256015 (archived):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/256015
- Microsoft KB Q138250 (archived):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/138250
- Microsoft KB Q161020, Implementing Windows 95 Updates:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/161020
- Windows 95 DDK `IFSMgr_Ring0_FileIO`:
  https://techshelps.github.io/MSDN/WIN95DDK/HTML/S312A.HTM

