# Windows ARM64EC Microsoft header ABI audit

**15 October 2026 — continuous Water / WHP ABI ledger.**
State: **CONFIRMED SOURCE-LEVEL SDK MACRO GAP PATCHED; FULL HEADER
GRAPH AND WINDOWS RUNTIME NOT YET REBUILT/EXECUTED**.
Continue, do not replace, the
[native ARM64 header audit](windows-arm64-header-abi.md),
[ARM64EC exception/unwind work](llvm-unwind.md),
[import-library audits](llvm-pe-coff-abi.md),
and the Win16/286 ABI separation.

## Microsoft contract, not compiler macro inference

Microsoft states that ARM64EC translation units compiling with
`windows.h` should have `_M_ARM64EC` (compiler target),
`_M_AMD64` and `_M_X64` (x64-compatible software ABI), **plus**
the Windows SDK-defined `_ARM64EC_` and `_AMD64_`. They
must **not** define `_M_ARM64` or `_ARM64_`.
This is intentional: x64 callers and ARM64EC callees share the
same Windows API data layouts, including x64 `CONTEXT`, despite
their different machine instructions and additional hybrid
entry/exit thunks.

An installed Clang 17 preprocessor probe confirmed that
`--target=arm64ec-pc-windows-msvc` provides
`_M_ARM64EC`, `_M_AMD64`, `_M_X64`,
`__arm64ec__` and `__x86_64__`, but does **not**
predefine `_ARM64EC_` or `_AMD64_`. The inspected Water
`include/windows.h`, `minwindef.h`, `basetsd.h`,
`winnt.h` and `ntdef.h` likewise did not supply them.
Consequently, programs using a Microsoft SDK architectural
`#ifdef _ARM64EC_` or `#ifdef _AMD64_` after including
Water's Windows headers could skip the correct implementation
or choose the wrong API structure branch.

## Targeted header correction

`include/basetsd.h`, already the common dependency of
`winnt.h` and the `windows.h` type graph, now supplies
the matching Windows SDK architecture markers for **Windows
PE** compilations:

| Windows target | `_AMD64_` | `_ARM64EC_` | `_ARM64_` |
| --- | --- | --- | --- |
| x64 | yes | no | no |
| ARM64EC | yes | yes | no |
| native ARM64 | no | no | yes |
| i386 | no | no | no |

The existing i386 `_X86_` remains untouched. All definitions
are guarded against previous definitions and excluded from
`WINE_UNIX_LIB`/non-PE host builds. They select SDK-style
declaration branches; they do **not** overwrite compiler
`_M_*` macros, change `__arm64ec__`, redefine the target
CPU, or provide an alternate calling convention.

## Already-correct structures preserved

`include/winnt.h` already provides an x64 `CONTEXT` to
ARM64EC because Clang defines `__x86_64__` for that target.
Its separate `ARM64EC_NT_CONTEXT` overlays the x64 context:
`X0 -> RCX`, `X1 -> RDX`, `Sp -> RSP`, and `Pc -> RIP`.
The ARM64EC and x64 `RUNTIME_FUNCTION` are 12 bytes; native
ARM64 uses an 8-byte packed runtime-function entry. These are
different contracts, and the repair retains them.

Negative finding: `include/msvcrt/intrin.h` places
`__arm64ec__` in its Arm load-acquire/store-release path and
explicitly excludes ARM64EC from including `x86intrin.h`;
its x86 `__cpuid` inline-assembly path follows an `#elif`
and therefore does not compile for ARM64EC. Do not rewrite
this into an x86 intrinsic/emulation layer without specific
failing evidence. See the preexisting `_umul128` and
128-bit shift-intrinsic audits.

**Separate, unresolved SDK mismatch:** Water's public
`include/winnt.h` defines
`IMAGE_FILE_MACHINE_ARM64EC = 0xA641` and
`IMAGE_FILE_MACHINE_ARM64X = 0xA64E`.
Microsoft says **A641 is not defined in its public winnt.h**:
it is a COFF `.obj`/`.lib` machine identifier, not a
valid final EXE/DLL PE machine value. Water may use these
constants in its object/archive tooling; the current change
does not delete them without a cross-repository consumer
audit. Do not mistake a public macro's presence for proof
that a PE image with machine `0xA641` is valid. This is
retained as an **OPEN compatibility divergence**, not a
silently resolved issue.

## Regression and verification boundaries

`scripts/tests/arm64ec-winheaders-abi.c` asserts architecture
macros and context/exception/memory layouts for ARM64EC, x64,
native ARM64 and i386. The wrapper
`scripts/tests/arm64ec-winheaders-abi.sh` compiles the real
Water `windows.h` graph for all four MSVC PE targets:

```sh
sh scripts/tests/arm64ec-winheaders-abi.sh
```

The direct installed Clang 17 preprocessor macro check
was run. The full Water header fixture and a freshly
configured Water build were **not** executed in this
environment. No Microsoft SDK compiler or ARM64EC Windows
process/runtime test was run. Header layout asserts are
necessary but cannot alone verify exception unwinding,
hybrid thunk stack transitions or runtime behavior.

## Sources

- Microsoft Windows team, ARM64EC preprocessor contracts:
  https://techcommunity.microsoft.com/blog/windowsosplatform/getting-to-know-arm64ec-defines-and-intrinsic-functions/2957235/
- Microsoft ARM64EC ABI and CONTEXT overlay:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
- Microsoft ARM64EC image vs object-machine distinction:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec
- Microsoft Windows ABI LLP64 model:
  https://learn.microsoft.com/en-us/windows/win32/winprog64/abstract-data-models
