# PIFMGR.DLL and Program Information Files (Windows 98 FE)

Status: **HISTORICAL EVIDENCE RECOVERED; SHELL32 NAMED-BLOCK WRITES
SOURCE-IMPLEMENTED; PIFMGR.DLL, PIPARSE.DLL, DOS VM INTEGRATION
NOT IMPLEMENTED OR RUNTIME-VERIFIED**.

## FE provenance and older history

Windows 98 FE's 1998 installed-system file inventory lists
`PIFMGR.DLL`, **82,832 bytes**, version **4.10.1998**, date
**May 11, 1998**. Its DMF floppy distribution lists the same
82,832-byte DLL in `WIN98_32.CAB` (disk 27), dated May 1, 1998,
beside `PIPARSE.DLL` (**61,440 bytes**). Do not silently equate
cabinet and installed timestamps.

Microsoft's Raymond Chen confirms `PIFMGR.DLL` began with
**Windows 95**, managing Program Information Files and containing
assorted icon resources used by DOS-program shortcuts. These icon
resources are not yet recovered/replicated in Water.

Sources:
- https://jeffpar.github.io/kbarchive/kb/197/Q197017/
- https://helparchive.huntertur.net/document/106792
- https://devblogs.microsoft.com/oldnewthing/20251020-00/?p=111706

## File structure and semantics

A .PIF is **not an executable** and is not synonymous with a .LNK.
It supplies launch-time configuration for a DOS program; applying
configurations ultimately belongs to the DOS VM / Win9x launch path,
not a filename-only DLL. The file format predates Windows 95 and
has a **0x171-byte base section** for title, program path, working
directory, command-line arguments, memory limits and display fields.
At byte 0x171 the Microsoft extension chain begins with
`MICROSOFT PIFEX`; later records include `WINDOWS 286 3.0`,
`WINDOWS 386 3.0`, `WINDOWS VMM 4.0`, `WINDOWS NT 3.1`,
and `WINDOWS NT 4.0`. Their binary contents and meaning must be
tested by OS personality instead of treating all versions as one.

See original file format documentation:
https://www.fileformat.info/format/pif/corion.htm

Microsoft documents a separate family
`PifMgr_OpenProperties`, `PifMgr_GetProperties`,
`PifMgr_SetProperties`, and `PifMgr_CloseProperties` in
**SHELL32.DLL 5.0+** with formal minimum support at Windows 2000.
They are not automatically proven *exports of PIFMGR.DLL in FE*.
Water already carries PifMgr APIs in **SHELL32.DLL ordinals
9, 10, 11 and 13**, according to the Win95-oriented spec.
Keep DLL ownership, shell versions and ordinals separate until
actual FE export tables are inspected.

- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-pifmgr_openproperties
- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-pifmgr_getproperties
- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-pifmgr_setproperties
- https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-pifmgr_closeproperties

## Targeted Water implementation

The existing `dlls/shell32/pifmgr.c` already opens PIF data,
follows record chains and reads named extensions. The formerly
stubbed **SHELL32 ordinal 11** now accepts **only existing named
extensions of the exact same byte length**; it holds the edited
contents in memory until close. Closing with `CLOSEPROPS_DISCARD`
rejects the edit. Normal close reopens the *same* file and checks
that its full original byte sequence is still unchanged before
rewriting it. A failed save leaves the property handle available.

This **does not** construct a new PIF file, insert or grow records,
serialize undocumented ordinal groups, or supply _Default.pif.
The write path isn't transactional across power failures.
Use fixtures/copies, not irreplaceable original DOS shortcuts,
until compatibility and durability are validated.

The `get_record` parser now requires the `MICROSOFT PIFEX`
signature before reading a linked extension chain. Bare legacy
TopView PIFs are valid but have no named Microsoft extensions.

The existing `dlls/shell32/tests/pifmgr.c` test now covers
fixed-size updates, persistence, unknown group/size rejection,
discard-on-close, and preservation of adjacent extension blocks.
**These tests are committed but not executed.**

## Outstanding separate components and gates

- `PIFMGR.DLL` (FE module itself): PE exports, icon group IDs,
  icon assets, properties UI and resource dependencies **unknown**.
- `PIPARSE.DLL`: file version, exports, function boundary with
  `PIFMGR.DLL`, and the 386/VMM section interpretations **unknown**.
- Windows 98 DOS VM: memory (conventional/EMS/XMS), working
  directory, environment, graphics/full-screen, idle scheduling,
  hotkey, CONFIG.SYS/AUTOEXEC.BAT support need actual behavior tests.
- Win9x `SHELL32` ordinal differences from NT shell32 5.0,
  16/32-bit thunks, corrupt record chains and random file input
  need separate verification. An exported symbol does not prove
  a working DOS-session launch.

Next: inspect genuine licensed Windows 98 FE binaries, record
hashes, native exports/resources and compare Win95/98/Me variants.
Then run Water's PIF tests and a genuine Win98 FE DOS-program
launch matrix before reporting functional PIFMGR.DLL compatibility.
