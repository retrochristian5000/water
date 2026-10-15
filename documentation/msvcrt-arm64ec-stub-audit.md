# MSVCRT.DLL ARM64EC export, stub and unwind-provider audit

**15 October 2026 — World History Project / Water.**
State: **CONFIRMED ARM64EC EXPORT IMPLEMENTATION GAP REPAIRED;
CLANG 17 COFF PROBE PASSED; REBUILT WATER AND WINDOWS RUNTIME UNVERIFIED**.
This continues the existing `__cdecl`, `/EHc`, Windows SDK
headers, ARM64EC unwind and import-library audits. Do not mix
Windows PE ARM64EC with classic ARM64, x86 Win16 or Apple arm64e.

## Scope and source hierarchy

The audited implementation is Water's
`dlls/msvcrt/msvcrt.spec`, `dlls/msvcrt/Makefile.in`,
`dlls/msvcrt/except.c`, `except_arm64ec.c`,
`except_x86_64.c`, `except_arm64.c`, and CRT headers.
It is **not** a dump of a genuine Microsoft MSVCRT.DLL binary
and does not prove that every symbol matches a specific
Windows 11 release.

Keep the roles of **msvcrt.dll** (historical CRT),
**ucrtbase.dll** (Universal CRT), **vcruntime140.dll** and
**vcruntime140_1.dll** distinct. An ordinary `@ cdecl`
entry or an export forwarded to `ucrtbase` is **not a
stub**. Lines beginning `# stub` in `msvcrt.spec`
are **comments**, not live exports.

## Verified export implementation hole: _local_unwind

`msvcrt.spec` advertises:

```
@ cdecl -arch=win64 _local_unwind(ptr ptr)
```

The implementation in `except_x86_64.c` is excluded by
`#if defined(__x86_64__) && !defined(__arm64ec__)`.
Before this patch, `except_arm64ec.c` had no
`_local_unwind` definition. That disconnects the
declared 64-bit MSVCRT export from its ARM64EC source
implementation.

**Patch:** `except_arm64ec.c` now provides:

```c
void __cdecl _local_unwind( void *frame, void *target )
{
    RtlUnwind( frame, target, NULL, 0 );
}
```

This is deliberately identical to the existing x64
delegation. Use Windows `RtlUnwind` to interpret
architecture-specific unwind data and ARM64EC/x64
transition context. Do **not** call Itanium
`_Unwind_*`, reinterpret the frame as classic
ARM64, or generate a dummy helper.

An isolated Clang 17 cross-compile confirmed:
- x64 COFF `0x8664`: definition `_local_unwind`,
  unresolved `__imp_RtlUnwind`.
- ARM64EC COFF `0xA641`: definition
  `#_local_unwind`, x64-facing alias
  `_local_unwind`, `$ientry_thunk$cdecl$v$i8i8`,
  unresolved `__imp_RtlUnwind` and the usual OS
  hybrid dispatch dependencies.

These are *object code and calling-boundary evidence*,
not proof of Windows-on-Arm runtime unwind correctness.

The regression
`scripts/tests/msvcrt-arm64ec-unwind-export.py`
reads **the actual** `_local_unwind` bodies from
both platform source files, checks the Win64 export,
cross-compiles them with equivalent Windows
`RtlUnwind` declarations, and inspects the COFF
machine, symbols and thunk dependencies:

```sh
python3 scripts/tests/msvcrt-arm64ec-unwind-export.py
```

It requires an actual Water checkout with
`clang-cl` and `llvm-objdump`; its committed copy
**has not yet been run**. An equivalent manually
constructed Clang fixture was run and passed.

## Active stubs: do not overcount

At this revision, there are **20 active**
`@ stub` lines in `msvcrt.spec`. **Seven**
are explicitly `-arch=i386` and are not applicable
to ARM64EC: `__p__fileinfo`, `_inp`,
`_inpd`, `_inpw`, `_outp`, `_outpd`,
`_outpw`.

The other **13** have no i386-only architecture
restriction. They are candidates for availability
checks, **not established ARM64EC-specific ABI
failures**:

| Stub group | Exported names | Status |
| --- | --- | --- |
| Locale data | `__lc_collate` | **OPEN**: investigate whether Microsoft exports function or data and what versions require it; do not silently substitute a function for data |
| Heap introspection | `_heapused` | **OPEN**: compatibility functionality, no ARM64EC-specific mismatch demonstrated |
| Multibyte classification | `_ismbbalnum`, `_ismbbalpha`, `_ismbbgraph`, `_ismbbkalnum`, `_ismbbkprint`, `_ismbbkpunct`, `_ismbbprint`, `_ismbbpunct` | **OPEN**: source-language/locale behavior common to several Windows ABIs |
| Multibyte collation | `_mbsncoll`, `_mbsnicoll` | **OPEN**: locale ordering, not calling convention proof |
| Legacy system time | `_setsystime` | **OPEN**: version/export and privilege-sensitive behavior |

Commented `# stub` lines are not counted as active
exports, even though they may identify genuinely missing
APIs for an older version of Microsoft's CRT. Each such
entry needs its own ABI/version/provenance check.

## Implemented exports with stub behavior

Some functions exist but have unresolved semantics:

- `except_arm64ec.c:handle_fpieee_flt` currently
  logs FIXME and always returns
  `EXCEPTION_CONTINUE_SEARCH`. The x64 and classic
  ARM64 implementations are also incomplete.
  Microsoft specifies that `_fpieee_flt` invokes
  the application's IEEE trap callback and returns
  the callback's status; it also supplies an
  `_FPIEEE_RECORD` describing the exception.
  Fabricating this record or invoking the callback
  with uninitialized operands would be unsafe.
- `except.c:__intrinsic_abnormal_termination`
  always returns 0, despite Microsoft's documented
  distinction between abnormal unwinding and normal
  execution of `__finally` blocks. Since Microsoft
  treats `AbnormalTermination` as an intrinsic in
  normal C/C++ SEH, this generic exported fallback
  must be investigated with an actual unwind fixture
  before changing its behavior or applying a TLS hack.
- `except.c` also records incomplete nested C++
  exception cleanup. The separate ARM64EC handler
  paths and `dlls/ntdll/unwind.c` still require
  runtime SEH/unwind cross-architecture tests.

These are **real open functionality concerns**,
not proofs that ARM64EC C++ symbol decoration or
runtime SEH should be replaced with newer LLVM
Itanium `libunwind`.

## Next verification steps

1. Run the new regression against a checked-out
   Water source and pinned LLVM fork, then link
   actual `msvcrt.dll`/import libraries for
   ARM64EC and x64. Validate the resulting exports
   using `llvm-readobj --coff-exports`.
2. On Windows 11 Arm, exercise
   `_local_unwind` in controlled SEH unwinding
   through native ARM64EC and x64 transition frames,
   checking `.pdata`/`.xdata` and
   `RtlVirtualUnwind2`/dispatch helper resolution.
3. Compare unresolved active stub names and
   function-vs-data kinds with versioned Microsoft
   CRT DLL export inventories before replacing
   any other entry.

## Primary sources

- Microsoft ARM64EC ABI (context and thunks):
  https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
- Microsoft ARM64EC module compatibility:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec
- Microsoft `_fpieee_flt`:
  https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fpieee-flt
- Microsoft `_abnormal_termination`:
  https://learn.microsoft.com/en-us/cpp/c-runtime-library/abnormal-termination
