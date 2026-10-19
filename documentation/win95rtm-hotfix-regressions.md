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

