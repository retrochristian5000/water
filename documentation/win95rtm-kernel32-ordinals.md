# Windows 95 RTM KERNEL32 export ordinals

This ledger is source evidence and a build-target compatibility policy, **not**
a claim that ordinals are stable between the Win9x and Windows NT families.
Water keeps system ABI declarations and export definitions; WHP-Drivers keeps
non-stock driver implementations.

| Binary / build | Decimal ordinal | Hex ordinal | Name | Evidence | Status |
| --- | ---: | --- | --- | --- | --- |
| Windows 95 RTM KERNEL32.DLL, guest-observed | 50 | 0x0032 | AddAtomA | Dependency Walker observation in the WHP; previously corroborated | i386 export pinned |
| Windows 95 RTM KERNEL32.DLL, guest-observed | 728 | 0x02D8 | WinExec | Dependency Walker observation in the WHP; 2000 Wine spec corroboration | i386 export pinned |
| Windows 95 KERNEL32.DLL, version not established from listing | 730 | 0x02DA | WinExec | Separate published export inventory | Conflict retained, NOT used as RTM evidence |

## Sources and limitations

- The January 2000 Wine `relay32/kernel32.spec` preserves `728 stdcall WinExec(str long) WinExec`, alongside `727 WideCharToMultiByte` and `729 WriteConsoleA`:
  https://sources.debian.org/src/wine/0.0.20000109-3/relay32/kernel32.spec
- A separate Windows 95 export inventory lists WinExec at hexadecimal
  `2DA` (decimal 730), with `2D8` assigned to WaitNamedPipeW:
  https://www.frankcheng.com/win32/win95.htm
- Another historical report notes GetDiskFreeSpaceExA/W appearing at ordinals
  316/317 on an OSR2-era system; this could explain a two-position difference,
  but **no causal link between those additions and WinExec's ordinal has yet
  been established**:
  https://groups.google.com/g/comp.emulators.ms-windows.wine/c/gUJhsD0hqkk
- These are *PE Win32 KERNEL32* ordinals, not ordinals from 16-bit KERNEL /
  KRNL386 or VxD service numbers. Do not conflate them.
- For historical imports, use function-name imports where the binary permits;
  ordinal imports depend on the exact library build. A single static Water
  i386 export table cannot simultaneously emulate contradictory RTM and
  OSR2 mappings for the same ordinal.

## Current Water implementation

- `dlls/kernel32/kernel32.spec`: i386 ordinal 50 -> AddAtomA and i386
  ordinal 728 -> WinExec; named, automatically numbered exports retained on
  non-i386 targets.
- `dlls/kernel32/tests/atom.c`: validate name and ordinal resolution; the
  WinExec check is gated to Win95 RTM-family version 4.00.950 and does
  **not** start an executable.
- Compiled PE export tables and Windows 95 guest execution still require
  independent verification. Also verify the system DLL file version and
  provenance when expanding the ledger to later Win9x releases.

## Dependent module observations: Win95 RTM vs Water build

An actual Windows 95 RTM guest running Dependency Walker shows
`USER32.DLL` referring to `KERNEL32.DLL`. This is an **observed module
dependency**, separate from the two guest-observed KERNEL32 export ordinals
above. The imported symbols (name versus ordinal), dependency edge type,
and exact file version/PE timestamp have **not yet been recorded**. Do not
infer that USER32 itself imports AddAtomA (50) or WinExec (728).

| Consumer | Provider | Basis | Classification |
| --- | --- | --- | --- |
| Native Win95 RTM `USER32.DLL` | Native `KERNEL32.DLL` | Dependency Walker report from the WHP RTM guest | Observed dependency; specific imports pending |
| Water `dlls/user32` | `$(PNG_PE_LIBS) gdi32 sechost advapi32 kernelbase win32u uuid` | `dlls/user32/Makefile.in` `IMPORTS` | Water build declaration, **not** Win95 RTM import evidence |
| Water `dlls/kernel32` | `kernelbase ntdll` | `dlls/kernel32/Makefile.in` `IMPORTS` | Water build declaration |
| Water `dlls/user.exe16` | `mpr user32 gdi32 win32u` | `dlls/user.exe16/Makefile.in` `IMPORTS` | Wine Win16 compatibility module, not native Win95 `USER32.DLL` |
| Water `dlls/gdi32` | `user32 advapi32 win32u` | `dlls/gdi32/Makefile.in` `IMPORTS` | Water build declaration |

**Important:** These Makefile lists are link inputs, not a dump of an
actual compiled PE Import Address Table. They must not be silently copied
into a Windows 95 RTM profile. In particular, adding `kernel32` to
Water's current `user32` `IMPORTS` would change the modern build graph
without establishing which Windows 95 interfaces the real DLL imported.
Avoid NT-only `KERNELBASE` assumptions in Win95 driver ABI declarations,
but do not remove it from current Water builds merely to reproduce the
1995 dependency list.

### Reproducible import-table audit

On a modern development host, with a legitimately obtained copy of the
guest's system DLLs (do **not** replace, redistribute, or execute them):

```sh
winedump dump -j import USER32.DLL
winedump dump -j export KERNEL32.DLL
```

Water already supplies `tools/winedump`; no extra PE parser is needed.
If building from the repository, use the locally built executable path.
In Dependency Walker on Win95, select `USER32.DLL`, then inspect the
imported-functions view for `KERNEL32.DLL`. For each row capture:

- original importing file name, Windows version, file version/PE timestamp;
- imported target DLL and **exact API name or raw ordinal**;
- whether the import is implicit, delay-load or dynamically discovered;
- corresponding KERNEL32 export name and ordinal in the **same guest build**;
- mismatches, absent imports, and unknown fields without filling the gaps.

The module dependency alone does **not** show that USER32 directly invokes
KERNEL32 ordinal 50 or 728. Even a by-name import has an ordinal in the
provider's export table, but it does not require that ordinal in the
consumer's import lookup. Keep Win16 `USER.EXE` and `KERNEL` ordinal
namespaces separate from Win32 `USER32.DLL` and `KERNEL32.DLL`.

Dependency Walker documents the difference between implicit, delay-load,
and explicit/dynamic dependencies:
https://www.dependencywalker.com/help/html/interpreting_errors.htm

Microsoft's 1995 Windows 95 API guidance separately warns against using
new Win32 APIs through undocumented Win16 exports:
https://helparchive.huntertur.net/document/37836

**Status:** guest-observed dependency plus source-audited Water build
difference. Native `USER32.DLL` function-level import table, compiled
Water PE imports, and runtime compatibility remain unverified.
