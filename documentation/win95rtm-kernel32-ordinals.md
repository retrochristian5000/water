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
