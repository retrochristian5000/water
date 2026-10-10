# WIN.INI parsing ledger — Windows 9x and Win16

Status: QUOTE-BOUNDARY REPAIR / REGRESSION TESTS ADDED /
COMPLETE WINDOWS 98 FE BEHAVIOR NOT YET VERIFIED.

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
- The parser is not responsible for executing [windows] load/run
  startup entries. No new startup behavior is claimed here.

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

## Unresolved questions

1. Compare Win98 FE-specific GetProfile* return and truncation quirks.
2. Audit subsystem-owned [windows] keys (load/run) at the appropriate
   Win9x shell startup boundary, without unconditional execution.
3. Preserve IniFileMapping distinction between Win9x/NT profiles.
4. Check profile comments, sections, code pages, double NUL enumeration,
   default handling, buffer sizes, and cache invalidation.
5. Retain the prior Win16 [Compatibility] NE version threshold.
