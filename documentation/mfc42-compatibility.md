# MFC42.DLL compatibility ledger (Win32 i386)

## Recovered state

Water currently has no builtin MFC42.DLL or MFC42U.DLL under dlls/.
Its native PE loader (dlls/ntdll), DLL overrides, and MSVCRT exports exist,
but none of these is itself an MFC implementation. The winecfg builtin-only
list does not include mfc42 or mfc42u. Do not add a fake builtin DLL merely
to make LoadLibrary succeed.

MFC42.DLL is the ANSI/MBCS MFC 4.2 runtime, while MFC42U.DLL is the separate
Unicode build. Visual C++ 6.0 shipped MFC42.DLL with a 6.00.x file version;
DLL identity and file version are separate datapoints. Keep Visual C++ 5
and Visual C++ 6 evidence separate until ABI comparison supports merging.

## ABI boundaries

- **Win95/Win98/VC5/VC6 target:** i386, PE32, MSVC x86 decorated C++ exports
  and calling conventions. Do not mix with 16-bit NE, PE32+, or a host macOS
  ARM64/ARM64e ABI.
- **ANSI vs Unicode:** MFC42 and MFC42U are different DLLs, not aliases.
- **Runtime dependencies:** verify imported MSVCRT, KERNEL32, USER/GDI, OLE
  and related entry points against the chosen Water guest personality.
  MFC code must not be implemented in MSVCRT, ATL or NTDLL.
- **C++ ABI:** test class layout, vtables, MSVC exception handling, RTTI,
  CString behavior, and object ownership. A list of exported symbol names
  cannot prove behavioral equivalence.
- **MFC state:** CWinApp initialization/shutdown, module/thread state,
  dynamic creation, message maps, resources, and extension DLL interaction
  require runtime tests. Microsoft's MFC documentation describes distinct
  module-state requirements for MFC-based DLLs.

## Current reproducible probe

Use an existing, legitimately obtained, version-pinned native MFC42 binary.
Water does not bundle, download, or redistribute Microsoft's DLLs.

    python3 scripts/tests/audit_mfc42.py /path/to/mfc42.dll
    python3 scripts/tests/audit_mfc42.py /path/to/mfc42u.dll

The source-only analyzer validates MZ/PE structure, i386 PE32 DLL identity,
the export-table DLL name, basic named-export integrity, and imported DLL
names. It reports export counts and MSVC-style C++ name counts. It does not
validate a specific Microsoft export list, each imported function, licensing,
DLL initialization, Windows compatibility, or a GUI application's behavior.
A synthetic DLL can pass the **format** check without implementing MFC.

With the correct architecture and a native DLL installed inside the correct
32-bit Water prefix, diagnose an application's actual load failure:

    WINEDEBUG=+loaddll,+module WINEDLLOVERRIDES="mfc42=n;mfc42u=n" ./build/wine /path/to/legacy-app.exe

This is a **diagnostic** override example for applications requiring those
modules, not a recommendation to install global overrides for every app.
Distinguish missing binary, wrong machine, unresolved export/ordinal,
dependency resolution, initialization, and C++ exception errors.

## Next implementation gates

1. Record native binary provenance, version, hash, machine, named and ordinal
   exports, decorated C++ signatures, imports, and authentic baselines for
   MFC42.DLL and MFC42U.DLL independently.
2. Reconcile imported Win32/CRT entry points against Water's actual PE
   export set before altering the loader or MSVCRT.
3. Run i386 GUI startup, window creation, message dispatch and cleanup
   probes against native MFC on the selected Windows personality.
4. Fix only proven failures in their owning components with regression
   tests. Begin a builtin MFC implementation only when class ABI contracts
   and behavioral fixtures exist; never guess export signatures.

## References

- Microsoft: MFC Library Versions; TN011 Using MFC as Part of a DLL;
  MFC extension DLLs; Exported DLL Function Entry Points.
- Microsoft Knowledge Base Q194298: VC5/VC6 MFC42 version history.
- Water: dlls/ntdll/unix/loadorder.c, dlls/ntdll/loader.c,
  programs/winecfg/libraries.c, dlls/msvcrt/msvcrt.spec.
