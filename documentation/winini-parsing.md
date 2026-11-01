# WIN.INI parsing ledger — Windows 9x and Win16

Status: QUOTE-BOUNDARY AND FALLBACK/PATH REPAIRS; WIN3 PROGMAN
LOAD/RUN DISPATCH WIRED; AOL-RELATED ISDN INSTALLER EVIDENCE LOGGED.
COMPLETE WIN3/95/98 GUEST BEHAVIOR NOT YET VERIFIED.

## Ownership and prior findings

- dlls/kernel32/profile.c is the shared INI parser and implements
  GetPrivateProfile*, WritePrivateProfile* and GetProfile* APIs.
- dlls/krnl386.exe16/file.c owns the KERNEL Win16 profile wrappers.
  GetProfileInt16, GetProfileString16 and WriteProfileString16 explicitly
  use win.ini; low-level parsing remains shared.
- dlls/krnl386.exe16/task.c consumes the [Compatibility] section.
  A prior repair already limits compatibility flags to matching
  NE programs with expected Windows version less than 0x030a.
  Do not reset or silently weaken this gate.
- SYSTEM.INI driver mappings and MSDOS.SYS boot configuration are
  distinct inputs, not interchangeable WIN.INI sections.
- The shared parser is **not** responsible for executing [windows] load/run.
  `programs/progman/main.c` now owns the Win3 Program Manager startup
  dispatch; Win95/98 Explorer startup, NT WOW and third-party shells are
  separate owners and must not be forced through Program Manager.

## Confirmed quote-copy defect and repair

The old PROFILE_CopyEntry helper applied the output bound before
removing matched quotes. A quoted three-character value such as
load="ABC" lost a valid final character in an exact-fit four-WCHAR
buffer. A zero-length value such as run="" could write to buffer[-1]
when the caller's destination capacity was one WCHAR.

The fix removes matching delimiters first, then bounds the copy by
output capacity minus one; it terminates safely for length one and
does not touch the output for nonpositive length. Unmatched quotes
remain literal. memmove also handles overlapping input/output safely.

## Regression testing

In dlls/kernel32/tests/profile.c, a temporary .ini fixture with
[windows] and [Compatibility] exercises the Win32 ANSI and Unicode
APIs without modifying a real WIN.INI file. The tests cover exact
fit, truncation, single quotes and guard bytes around small outputs.

A standalone reproduction of the patched helper passed GCC C99
compilation with -Wall -Wextra -Werror and ASan/UBSan, including an
empty quoted value with a one-character destination. Full kernel32
build, Wine test execution and Windows 98 FE comparison remain pending.

## Additional profile-path and fallback repairs (November 1, 2026)

In `dlls/kernel32/profile.c`, GetPrivateProfileStringW previously
formed `def_val - 1` for an empty default string and unconditionally
dereferenced a trim-allocation result; it now trims via bounded lengths,
handles the empty/all-space cases, and preserves the original default
if allocation fails. The earlier matched-quote fix remains intact.

The shared profile path resolver also checks the GetWindowsDirectoryW
and GetFullPathNameW lengths before composing into a MAX_PATH buffer.
These protections apply to generic profile APIs, not a special AOL
parser. Existing `dlls/kernel32/tests/profile.c` now also checks empty
and space-only defaults plus raw [windows] section quotes. Full binary
validation remains pending.

## Windows 3.x Program Manager startup semantics

Microsoft Windows 3.x uses `WIN.INI` `[windows] load=` and `run=`
as **independent** startup lists. Windows 3.1 also has a Program
Manager Startup group selected through `PROGMAN.INI [Settings] Startup=`;
a blank Startup setting suppresses that group without deleting
WIN.INI run/load settings. Program Manager is the owner of the Win3
dispatch; Microsoft's KB Q85476 warns that an alternate shell may
ignore both the WIN.INI lists and the Startup group.

The new `programs/progman/main.c` dispatch consumes the raw WIN.INI
section (preserving quotes lost by GetProfileString on an entirely
quoted value) and starts each space-delimited filename with WinExec:
- `load=`: `SW_SHOWMINNOACTIVE` (minimized without activation);
- `run=`: `SW_SHOWNORMAL` (normal launch).
This follows Microsoft KB Q69844. The Startup-group path remains
separate, with its existing Shift/empty-group suppression. Quoted
filenames can contain spaces; no unsupported argument grammar was
invented. Long/truncated section reads are not executed. This is
**not** evidence that WATER boots DOS Windows 3.x, or that the
Win95/98 Explorer shell now processes WIN.INI.

## AOL / America Online installation evidence — scoped

AOL-related software really did change WIN.INI on **some** systems,
but do not generalize this to all AOL application versions.

The original cFos vendor configuration instructions describe
two AOL-associated ISDN support variants:
- **AOLISDN**: a `[windows] RUN=` entry such as
  `C:\\AOLISDN\\AOLISDND.EXE`; the accompanying SYSTEM.INI
  references include `VAOLISDN.VXD` and an `[AOLISDN]` section.
- **AOL-bundled OEMcFos**: a `[windows] run=` entry such as
  `C:\\WINDOWS\\OEMcFos\\cFosoemd.exe`; SYSTEM.INI may contain
  `device=vcFosoem.vxd` and `[cFosoem]`.

These are **vendor-documented removal targets**, not captured
before/after diffs from a particular AOL installer. The vendor says
'Recent AOL versions' for OEMcFos but does **not** identify an exact
AOL major/minor version in that passage. Consequently, mark the
precise AOL client version/edition, installation date, and whether
these components were installed by default as **unverified**.

AOL 3.0 Win16/Windows 3.1 and 32-bit Windows 95 installer branches
were already separated in the WHP audit; do not merge their setup
effects merely because both were marketed as '3.0'. A Trojan
masquerading as AOL components can also create RUN/LOAD entries:
the presence of an AOL path alone does not identify a legitimate
AOL installer. Recover original installer script/INF or a dated
before/after WIN.INI to resolve specific variants.

Sources:
- cFos original vendor troubleshooting and uninstall instructions:
  https://cfos.de/en/cfos-professional/app.htm
- Microsoft KB Q69844, load/run display semantics (Windows 3.0):
  https://jeffpar.github.io/kbarchive/kb/069/Q69844/
- Microsoft KB Q85476, Program Manager versus third-party shell:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/85476
- Microsoft KB Q81606, independent Startup group:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/81606
- F-Secure, Trojan.AOL.Buddy (negative control for attribution):
  https://www.f-secure.com/v-descs/aolbuddy

## Unresolved questions

1. Compare Win98 FE-specific GetProfile* return and truncation quirks.
2. Verify Win3 Program Manager load/run ordering and actual launches
   in a Windows 3.x guest; independently audit Win95/98 shell startup
   without unconditional Win3 Program Manager execution.
3. Preserve IniFileMapping distinction between Win9x/NT profiles.
4. Check profile comments, sections, code pages, double-NUL enumeration,
   small buffers, and cache invalidation.
5. Retain the prior Win16 [Compatibility] NE version threshold.
6. Recover exact original AOL installation builds or INF/script changes
   before assigning an ISDN RUN entry to a particular AOL version.
