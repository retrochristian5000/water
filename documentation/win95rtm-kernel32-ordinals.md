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


## USER32 -> GDI32 dependency: Windows 95 RTM metafile import

A Windows 95 RTM guest running Dependency Walker shows native `USER32.DLL`
referencing `GDI32.DLL`, with `SetMetaFileBitsEx` as an imported API.
This is a separate **guest-observed import label** from the earlier
`USER32.DLL -> KERNEL32.DLL!FT_Exit48` evidence. Record it without
assuming that the function actually ran, that it was imported by name
rather than by ordinal, or that all of USER32's GDI imports are known.
File-level PE import descriptors, file versions, and the RTM GDI32
`SetMetaFileBitsEx` export ordinal have **not** yet been captured.

| Consumer | Provider | Symbol | Evidence | Status |
| --- | --- | --- | --- | --- |
| Win95 RTM `USER32.DLL` | Win95 RTM `GDI32.DLL` | `SetMetaFileBitsEx` | Dependency Walker guest observation | Import label observed; ordinal / lookup encoding unverified |
| Water `dlls/user32` | Water `dlls/gdi32` | No specific name established by the Makefile | `dlls/user32/Makefile.in`: `IMPORTS` includes `gdi32` | Modern build link input; **not** evidence of exact RTM imports |
| Water `dlls/gdi32` | Water `dlls/user32` | No specific name established by the Makefile | `dlls/gdi32/Makefile.in`: `IMPORTS` includes `user32` | Modern reverse build dependency; actual PE imports unverified |

### ABI and implementation audit

- Water's `dlls/gdi32/gdi32.spec` exposes
  `@ stdcall SetMetaFileBitsEx(long ptr)`, currently without a fixed ordinal.
- Water's `include/wingdi.h` declares `HMETAFILE WINAPI
  SetMetaFileBitsEx(UINT, const BYTE *)`. On i386, the two arguments are
  4 bytes each with Win32 stdcall stack cleanup, **not** a Win16 thunk
  exit. This is a Windows-format metafile (WMF), not automatically an
  enhanced metafile (EMF).
- Water's `dlls/gdi32/metafile.c` implements it by copying the supplied
  Windows-format metafile data into a GDI-managed in-memory metafile.
  The prior code accessed `METAHEADER` fields for small, positive
  buffer sizes **before confirming the header fitted**; the new guard
  rejects truncated headers and NULL data with `ERROR_INVALID_DATA`
  before dereferencing the header. This does **not** validate all nested
  WMF records: record-walking bounds are a separate audit target.
- Regression checks in `dlls/gdi32/tests/metafile.c` cover truncated
  and NULL input under Water/Wine only. Historical native Windows
  versions can crash on NULL input, so the tests deliberately do not
  execute those inputs against the original RTM DLL.

The public Win32 API description identifies the function as creating
an in-memory *Windows-format metafile* from a byte buffer:

https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-setmetafilebitsex

That current documentation lists modern support baselines; they are
**not** evidence that Windows 95 RTM lacked the function when the
guest's import view says otherwise.

**Next proof points:** Extract the raw `USER32.DLL` import-table entry
for `SetMetaFileBitsEx`, the exact matching RTM `GDI32.DLL` export
ordinal/name, and the timestamps or hashes of both system DLLs. Then
compare them with a freshly built Water i386 PE import/export table.
Do not pin GDI32 ordinals, add an RTM-only build-time link cycle, or
invent a function call site before that evidence is available.

## Dependent module observations: Win95 RTM vs Water build

An actual Windows 95 RTM guest running Dependency Walker shows
`USER32.DLL` referring to `KERNEL32.DLL`. This is an **observed module
dependency**, separate from the two guest-observed KERNEL32 export ordinals
above. The guest also identifies `FT_Exit48` as a KERNEL32 import referenced
by USER32. The import lookup encoding (raw name versus ordinal), other imported
symbols, dependency edge type, and exact file version/PE timestamp remain
**unrecorded**. Do not infer that USER32 imports AddAtomA (50) or WinExec (728).

| Consumer | Provider | Basis | Classification |
| --- | --- | --- | --- |
| Native Win95 RTM `USER32.DLL` | Native `KERNEL32.DLL` | Dependency Walker report from the WHP RTM guest | Observed dependency; `FT_Exit48` import identified, remaining imports pending |
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

### FT_Exit48: observed import and thunking ABI

- **Guest observation:** Windows 95 RTM `USER32.DLL` lists `FT_Exit48`
  as an import from `KERNEL32.DLL` in Dependency Walker. This updates the
  previously unknown function-level edge. It is **not** evidence that
  `FT_Exit48` is a USER32 export, that USER32 imports it by ordinal,
  or that the function was actually executed. A successfully resolved
  dependency would corroborate its presence as a provider export.
- **Source-level cross-check:** Water's `dlls/kernel32/kernel32.spec`
  already includes
  `@ stdcall -i386 -private -norelay FT_Exit48() krnl386.exe16.FT_Exit48`.
  The Wine/WHP `-private` flag excludes ordinary import-library use; it
  does not by itself prove that the export is absent from the PE export
  directory. The underlying 32-bit callable relay is declared in
  `dlls/krnl386.exe16/krnl386.exe16.spec`.
- **Behavior and ABI:** `dlls/krnl386.exe16/thunk.c` implements the
  `FT_ExitNN` family. The generated `FT_Exit48` restores the
  thunk frame and saved registers, moves the result into EAX, then
  returns using `ret $48`, i.e. a **48-byte stack cleanup**. Despite
  the Win32 .spec entry's empty argument signature, this is a special
  compiler-generated thunk exit, **not** an ordinary zero-argument
  application API and not `ExitProcess`.
- **Historical cross-check:** A January 2000 Wine KERNEL32 export spec
  lists `229 register FT_Exit48() FT_Exit48`; a separate Windows 95
  export inventory also lists hexadecimal `0E5` (= decimal 229).
  This is a **candidate historical ordinal**, not an RTM-specific
  ordinal verified against the user's installed KERNEL32 binary.
  Do **not** pin or move Water's FT_Exit48 ordinal solely on this basis.

Sources:
- https://sources.debian.org/src/wine/0.0.20000109-3/relay32/kernel32.spec
- https://www.frankcheng.com/win32/win95.htm
- https://source.winehq.org/WineAPI/kernel32.html

**Follow-up proof targets:** Read `USER32.DLL`'s raw import thunk to
distinguish an imported name from a resolved ordinal label; inspect the
same guest's `KERNEL32.DLL` exported `FT_Exit48` ordinal and PE
timestamp/file version; then compare an actual Water i386 PE export
table. Avoid calling `FT_Exit48` as a test: it expects a prepared
thunk stack frame, so an ordinary direct call would be unsafe.


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

**Status:** guest-observed module dependency **and FT_Exit48 import label**;
source-audited Water export and 48-byte thunk exit implementation. Other
native USER32 function-level imports, import encoding, RTM FT_Exit48
ordinal, compiled Water PE imports, and runtime compatibility remain
unverified.
