# MSVCRT.DLL: Win32 / Win64 stat export ABI audit

Status: **SIX EXPORT ROUTES FIXED; BACKING FUNCTIONS ALREADY IMPLEMENTED;
WIN64 RUNTIME TEST ADDED, NOT YET EXECUTED** (2026-10-09).

## Ownership and ABI boundaries

- Windows 98 FE's MSVCRT.DLL contract is 32-bit x86. AArch64 is *not*
  a Windows 98 binary architecture. Do not silently attach Windows
  ARM64 (or Arm64EC) exports to the Windows 98 guest identity.
- Water's MSVCRT.DLL Wine-compatible implementation is shared across
  several Windows target architectures. Its .spec decides the public
  export name and which C implementation receives the call.
- In Wine .spec architecture filters, win32 covers 32-bit Windows
  targets, while win64 covers 64-bit Windows targets including Classic
  ARM64. Arm64EC has additional calling-convention and thunk rules;
  Win64 filters alone do not constitute complete Arm64EC validation.
- The host ABI (macOS aarch64-apple-darwin) is separate from the
  Windows ARM64 guest ABI (aarch64-windows-msvc).

## Six confirmed mismatched mappings

The Water fork previously exported each name without an architecture
mapping, directly to legacy _stat/_fstat/_wstat C functions. Upstream
Wine's architecture-specific definitions supply different layouts:

| Public name | Win32 backing implementation | Win64 backing implementation |
| --- | --- | --- |
| _fstat | _fstat32 | _fstat64i32 |
| _fstati64 | _fstat32i64 | _fstat64 |
| _stat | _stat32 | _stat64i32 |
| _stati64 | _stat32i64 | _stat64 |
| _wstat | _wstat32 | _wstat64i32 |
| _wstati64 | _wstat32i64 | _wstat64 |

All twelve functions already existed in dlls/msvcrt/file.c.
The six pairs are copied from the current upstream Wine msvcrt.spec,
with each existing public name preserved and without adding fake
exports. The separate _fstat64, _stat64 and _wstat64 exports are
unchanged. The stat64i32 structures have 64-bit timestamps and
32-bit size fields; stat64 structures carry 64-bit timestamps and size.
Incorrect dispatch risks wrong field offsets or writing beyond the
caller's structure size.

## Existing ARM64-relevant exports

- __C_specific_handler is already marked for !i386.
- _get_environ and _get_wenviron already exist and are implemented;
  they are the relevant arm/arm64 environment accessor route.
- ntdll.spec owns __chkstk and __chkstk_arm64ec. Compiler-specific
  stack-probe and Arm64EC __security_check_cookie thunks must not be
  invented in MSVCRT.DLL without proven ownership and ABI evidence.
- __p__environ and __p__wenviron are explicitly i386-only in
  MSVCRT.DLL. UCRTBASE is a distinct CRT with a different export set.

## Verification and next gates

Run the source alias audit:

    python3 scripts/tests/test_msvcrt_stat_exports.py

On a working 64-bit Windows Wine/Water test environment, run the
existing msvcrt file test target. dlls/msvcrt/tests/file.c now tests
all six GetProcAddress names and calls them with the Win64 expected
structures, checks file sizes, and places canaries after their buffers.
It does not touch host system files. The test is also valid for
Windows ARM64, subject to actual runtime availability.

The patched export pairs were confirmed against upstream Wine and
existing Water backing functions through source inspection. The
full Water build, spec linker validation and AArch64 guest execution
remain pending.

Later tasks:
1. Compile MSVCRT.DLL for i386, x86_64 and ARM64 Windows target ABIs.
2. Check export tables and C calling signatures for all three targets.
3. Run the native Windows ARM64 file, exception and environment tests.
4. Audit Arm64EC separately for call checkers and stack probes.
5. Compare the Windows 98 FE x86 MSVCRT binary independently; don't
   infer ARM64 support from the historical 9x distribution.

## Sources

- Water: dlls/msvcrt/msvcrt.spec, dlls/msvcrt/file.c,
  dlls/msvcrt/tests/file.c.
- Upstream Wine: https://github.com/wine-mirror/wine/blob/master/dlls/msvcrt/msvcrt.spec
- Microsoft Arm64EC ABI:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
- mingw-w64 ARM64 MSVCRT environment accessors:
  https://github.com/mingw-w64/mingw-w64/blob/master/mingw-w64-headers/crt/stdlib.h
