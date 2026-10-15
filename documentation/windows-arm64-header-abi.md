# Windows ARM64 headers: ABI alignment audit

For the separate x64-compatible hybrid personality, see
[ARM64EC Windows SDK architecture markers and context ABI](windows-arm64ec-header-abi.md).

## Scope and provenance (14 October 2026)

The target here is **ordinary AArch64 Windows with the MSVC PE/COFF
ABI** (`aarch64-pc-windows-msvc` / Water's `aarch64-windows`).
Keep it separate from 64-bit macOS/Unix hosts (LP64), ARM64EC
hybrid Windows, GNU/MinGW ARM64, and the Win9x i386 PE32 personality.
The Windows LLP64 contract preserves 32-bit `LONG`, `DWORD` and
`ULONG` while pointers, `SIZE_T`, `ULONG_PTR` and `LONG_PTR`
are 64-bit.

Microsoft reference contracts:
- [LLP64 Windows data model](https://learn.microsoft.com/en-us/windows/win32/winprog64/abstract-data-models)
- [MEMORY_BASIC_INFORMATION and explicit 32/64 variants](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-memory_basic_information)
- [ARM64_NT_CONTEXT](https://learn.microsoft.com/en-us/windows/win32/api/winnt/ns-winnt-arm64_nt_context)
- [Windows ARM64 ABI](https://learn.microsoft.com/en-us/cpp/build/arm64-windows-abi-conventions)
- [Windows ARM64 exception unwind records](https://learn.microsoft.com/en-us/cpp/build/arm64-exception-handling)
- [MSVC packing defaults and header constraints](https://learn.microsoft.com/en-us/cpp/preprocessor/pack)

## Confirmed header gaps and fixes

**1. Missing PartitionId member in native 64-bit memory information.**
Water's previous `include/winnt.h` definition of
`MEMORY_BASIC_INFORMATION` omitted the Microsoft SDK's
`WORD PartitionId` member. On ordinary ARM64/x64 Windows,
the field occupies bytes 20-21 in existing alignment padding,
between `AllocationProtect` (offset 16) and `RegionSize`
(offset 24). Restoring it under `_WIN64` keeps the native
64-bit structure at **48 bytes**, aligned to 8 bytes, and
preserves every later offset. The older 32-bit memory query
layout remains at **28 bytes** with `RegionSize` at offset 12.
The Win9x/i386 contract is deliberately not changed to match a
later SDK.

The old runtime did not initialize those padding bytes.
`dlls/ntdll/unix/virtual.c` now returns `PartitionId = 0`
on both local and cross-process successful basic-memory query
paths. Zero is Water's *default/no additional partition
information* behavior here, **not a claim that real Windows
always reports zero**. Allocation, state, protection and region
sizes remain unmodified.

**2. Missing explicit cross-architecture query records.**
Microsoft's SDK also supplies
`MEMORY_BASIC_INFORMATION32` and `MEMORY_BASIC_INFORMATION64`.
Water previously lacked both in `winnt.h`. They have now been
declared with fixed-width fields: 32-bit record size **28**,
64-bit record size **48**, and the latter's explicit **16-byte**
alignment plus documented `__alignment1` and `__alignment2`
fields. Their size and alignment must not depend on the caller's
compilation machine, so no host pointer type occurs in either
record. These types do not replace the native
`MEMORY_BASIC_INFORMATION` nor assert a Windows 9x capability
that did not exist.

## Other layouts inspected, not changed

| Contract | Expected ordinary Windows ARM64 |
| --- | --- |
| `WCHAR` | 2 bytes |
| `LONG`, `ULONG`, `DWORD` | 4 bytes |
| `SIZE_T`, `LONG_PTR`, `WPARAM`, `LPARAM` | 8 bytes |
| `OVERLAPPED` | 32 bytes; `hEvent` offset 24 |
| `EXCEPTION_RECORD` | 152 bytes; parameters offset 32 |
| `ARM64_NT_CONTEXT` | 0x390 bytes, 16-byte aligned |
| `ARM64_NT_CONTEXT.V` | offset 0x110 |
| `ARM64_NT_CONTEXT.Sp`/`Pc` | 0x100 / 0x108 |
| ARM64 `.pdata` runtime function entry | 8 bytes |
| ARM64 PE machine constant | 0xAA64 |

These expected values are encoded as compile-time assertions in
`scripts/tests/arm64-winheaders-abi.c`. They are **checks**, not
claims of Windows ARM64 runtime execution. The ARM64 `.pdata`
record is a compact 8-byte pair, unlike the x64 12-byte
runtime-function record; merging those layouts would break
unwinding.

Run in a checked-out Water source tree:

```sh
sh scripts/tests/arm64-winheaders-abi.sh
```

It cross-compiles the actual Water `windows.h` header graph
for MSVC ARM64, MSVC x64 and MSVC i686, without linking or
attempting to run Windows binaries. It uses the repo's public
and MSVCRT header search paths. An appropriate Clang and its
target-usable headers are required. `WHP_HEADER_CC` can select
the cross-compiler.

A separate **standalone Clang 17** probe compiled the
`MEMORY_BASIC_INFORMATION` and fixed-width record
declarations for ARM64/x64/i686, confirming offsets and
alignments with Clang's Windows MSVC data model. The complete
Water header-graph regression **has not yet been executed** in
the actual Water checkout. A native Windows ARM64 loader or
`VirtualQuery` runtime test has likewise not been run.
Distinguish those stages before declaring full ABI compliance.

## Continuing audit boundaries

1. A header can match byte offsets yet have incorrect function
   calling conventions, ANSI/Unicode aliases or symbol exports.
   Confirm against actual DLL exports and import libraries.
2. Never apply `-fpack-struct` or `/Zp1` globally to Windows
   SDK structures. ARM64's default structure member pack is 8;
   ARM64EC and x64 have distinct defaults.
3. ARM64EC has a separate `CONTEXT`/exception path and x64
   interoperability metadata; its headers should be validated
   independently instead of silently reusing ordinary AArch64
   results.
4. Older 32-bit Windows personalities retain their original
   structure contracts until age/version-specific behavior is
   demonstrated. New SDK fields are not evidence of Win98 APIs.

This ledger distinguishes Microsoft-documented structure
contracts, Water's source-level implementation changes, and
independently compiled fixture checks.
