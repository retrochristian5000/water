# MSVC __cdecl: ARM64EC, x64, x86, import archives and varargs

**15 October 2026 — Water / World History Project ABI ledger.**
State: **DIRECT CLANG/LLD 17 REPRODUCTION PASSED; TEST COMMITTED;
ARM64EC RUNTIME AND FRESHLY BUILT WATER/LLVM UNVERIFIED.**

This continues the established `/Gv`, `/MACHINE`, `/DLL`,
`/EHc` and ARM64EC import-library work, retaining negative
evidence and the separate Win16/286 protected-mode boundary.
No compiler calling convention or export mangling was changed
without a reproducible mismatch.

## Microsoft contract and semantic split

On 32-bit **x86**, `__cdecl` is the default C calling
convention: stack arguments, caller stack cleanup, and C-linkage
external `_name` symbol decoration in COFF object files. It
differs from `__stdcall`'s `_name@bytes` and
`__vectorcall`'s `name@@bytes`. An explicit `__cdecl`
overrides `/Gv`, `/Gr`, and `/Gz` where supported.
Export-table names may omit the object-level leading underscore:
do not confuse a DEF file's public export spelling with the
internal COFF symbol.

On **x64**, `__cdecl` is accepted and ordinarily uses the
platform's x64 register calling convention and undecorated
C-linkage name. It matters when it overrides the explicit
`/Gv` vectorcall default. On **native ARM64**, the ordinary
ARM64 calling convention and undecorated name apply.
On **ARM64EC**, C-linkage ARM64EC definitions use a **`#`**
prefix while x64-compatible alias/import/export names
remain available; C++ decorated names insert `$$h`.
ARM64EC follows x64-compatible *software* conventions
but not the classic x64 instruction/register layout:
entry/exit thunks bridge differences per signature.

| ABI | Example C `__cdecl whp_cdecl` definition |
| --- | --- |
| i386 (PE32) | `_whp_cdecl` |
| x86-64 (PE32+) | `whp_cdecl` |
| native ARM64 | `whp_cdecl` |
| ARM64EC | `#whp_cdecl` plus weak x64-facing `whp_cdecl` |

For the same C++ `int whp_cpp(int, double)`,
x64 uses `?whp_cpp@@YAHHN@Z` and ARM64EC uses
`?whp_cpp@@$$hYAHHN@Z`. This is correct Microsoft
ABI metadata, not evidence of a defective Clang mangler.

### Why variadic __cdecl is a sharper test

Microsoft's ARM64EC ABI specifies a **special varargs
calling convention**: only the first four register argument
slots are used, with further arguments on the stack, and
floating-point arguments carried through general-purpose
registers rather than the usual SIMD register placement.
Native Windows ARM64 does not follow those exact rules.
Clang's ARM64EC assembler/object output creates a
`$ientry_thunk$cdecl$i8$varargs` for variadic entries
and a `$iexit_thunk$cdecl$i8$varargs` for imported
variadic calls. A nonvariadic `(int,double)` has a
different `$ientry_thunk$cdecl$i8$i8d`. Flattening
these signatures or substituting native ARM64 procedure
conventions would be a real ABI violation.

## Verified COFF object and import-library data

Ran Clang/clang-cl 17, llvm-objdump 17 and LLD 17
on the same `__cdecl` C/C++ fixture across x86, x64,
native ARM64 and ARM64EC. The symbols matched the
table above, including the C++ `$$h` insertion.
An explicitly `__stdcall` x86 function instead
emitted `_whp_stdcall@12`; a `__cdecl` varargs
definition retained its cdecl name. A `dllimport`
cdecl-varargs call used `__imp__whp_external` on
i386 and `__imp_whp_external` on x64/ARM64/ARM64EC.
The ARM64EC object carried separate cdecl entry/exit
thunks, plus a weak undecorated x64-facing alias.

With `/Gv`, the unqualified x64 function became
`whp_default@@16` while explicit `__cdecl whp_cdecl`
remained `whp_cdecl`. ARM64EC retained ordinary
`#whp_default` and `#whp_cdecl`: do not interpret
this as ARM64EC vectorcall support.

`lld-link /lib /machine:arm64ec` built two short
function-import records with `0xA641` and function
symbol names `#whp_cdecl` and `#whp_vararg`.
The records retained undecorated DLL entry names
`whp_cdecl` and `whp_vararg`. Its archive included
the specialized `/<ECSYMBOLS>/` symbol index.
The otherwise equivalent `/machine:x64` archive
used machine `0x8664` with ordinary function names
and no ARM64EC index. These records validate the
already-repaired Water ARM64EC `/lib` producer
contract, not Windows runtime call execution.

**Standalone test:** `scripts/tests/cdecl-arm64ec-abi.sh`
was added to prevent future mangling or calling-convention
regressions.

```sh
sh scripts/tests/cdecl-arm64ec-abi.sh
```

The corresponding standalone fixtures were executed
successfully in this round. The committed copy is
source-verified but **not yet rerun against the built
Water/LLVM fork**. The test intentionally does not
create a fake runtime thunk or claim successful Windows
ARM64EC execution.

## Current Water and LLVM source status

- `tools/winebuild/utils.c:get_abi_name()` applies
  stdcall suffix transformations only to **i386**;
  ordinary `TYPE_CDECL` remains unmodified. This is
  consistent with Microsoft x64 and ARM64EC. Do not
  add a `_name` or `@bytes` convention to ARM64EC.
- The prior `/Gv` round fixed i386 `name@@N`
  being parsed as stdcall; that distinction remains
  necessary but does not imply a new ARM64EC defect.
- Windows PE compilation through `winegcc` uses
  Clang's MSVC target calling-convention handling,
  not a forced `__cdecl=__stdcall` substitution.
  The `gcc_defs` compatibility macro branch is
  used in non-PE paths; its CPU_ARM64EC case is not
  evidence of a PE ABI mismatch.
- LLVM 17 already emits the tested ARM64EC-specific
  cdecl and varargs thunks. No source-level calling
  convention change was justified. Do not change
  x64/ARM64EC ABI tags or `__imp_` names without a
  failing cross-architecture fixture.

## Unverified boundary and negative evidence

Directly linking a standalone ARM64EC function
without the complete OS/hybrid runtime gave an
undefined `__os_arm64x_dispatch_ret` reference
(and, for some functions, security-cookie helpers).
**This is a required transition-to-x64 emulator
helper**, documented by Microsoft. It is not a
reason to replace it with a native ARM64 return or
to manufacture a dummy implementation. A real
Windows-on-Arm runtime and PE `.pdata`/`.xdata`
unwind validation remain future work. Neither the
native ARM64 object nor an x64-only import library
can by itself demonstrate correct cross-ABI call
and return behavior.

Intel **80286 protected mode** remains a separate
16-bit segmented ABI with far returns, descriptors
and selector rules. The PE32 x86 `_name` cdecl
convention and the ARM64EC thunk identities must
not be mapped to 286 execution semantics.

## Primary references

- Microsoft `__cdecl` keyword:
  https://learn.microsoft.com/en-us/cpp/cpp/cdecl
- Microsoft ARM64EC ABI conventions:
  https://learn.microsoft.com/en-us/cpp/build/arm64ec-windows-abi-conventions
- Microsoft low-level Arm64EC and variadic/thunk details:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
- Microsoft decorated names:
  https://learn.microsoft.com/en-us/cpp/build/reference/decorated-names
