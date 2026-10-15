# MSVC `#pragma conform(forScope)`: Windows ARM64EC compatibility boundary

**15 October 2026 — continuous Water/LLVM ABI ledger.**
State: **SOURCE AUDIT AND WRAPPER GUARD PUBLISHED;
CLANG 17 FOUR-ARCHITECTURE PROBES PASSED; REBUILT WATER UNVERIFIED.**
This audit builds on the `/Gv`, `__cdecl`, `/EHc`,
`/arch`, Windows SDK, COFF and ARM64EC unwind studies.
Do not reset their evidence or borrow assumptions from Win16/286.

## The documented Microsoft behavior

Microsoft's [`conform` pragma](
https://learn.microsoft.com/en-us/cpp/preprocessor/conform)
is C++-specific and only accepts **`forScope`** as its setting.
It controls the source-language lifetime/visibility of a
variable declared by a `for` initializer, corresponding to
`/Zc:forScope`. `on` makes the variable inaccessible once
the loop exits; `off` preserves Microsoft legacy extension
visibility in the enclosing scope. The pragma also offers
`show` (MSVC warning C4811) and named `push`/`pop`
state management. Microsoft documents modern
[`/Zc:forScope`](https://learn.microsoft.com/en-us/cpp/build/reference/zc-forscope-force-conformance-in-for-loop-scope)
as enabled by default. `/Zc:forScope-` is deprecated and
emits warning D9035 in supported MSVC environments.

**This is not an ARM64EC ABI switch.** It does not set x64
stack shadow space, argument-register marshalling, vectorcall,
`__cdecl`, `$$h` C++ name mangling, PE header machine types,
pointer width, C++ exception personality, `.pdata/.xdata`,
`#pragma pack` alignment or ARM64EC entry/exit thunks.

Possible *indirect* effect only: legacy scoping can change
whether C++ code is well-formed and which name is resolved
in a function body. It cannot justify different public
function parameter layouts or symbol decoration for ARM64EC.

## Reproduced Clang 17 compatibility gap

The installed Clang/clang-cl 17:
- Treats `#pragma conform(forScope, on|off|show)`
  as an **unknown pragma** on i686, x86-64, native ARM64
  and ARM64EC Windows MSVC targets.
- With `/W0`, can silently ignore the directive. With
  `/W4` it warns; `/clang:-Werror=unknown-pragmas`
  makes the unsupported setting an explicit build failure.
- Treats `/Zc:forScope-` as an **unused argument**.
  It does **not** restore legacy loop-initializer scope:
  referring to `i` after `for (int i = ...)` errors
  on every tested target.
- Compiles valid C++ with the `conform` pragma ignored
  into the same conventional COFF C symbol as without
  the pragma: `_whp_conform` on i686, `whp_conform`
  on x64/native ARM64, `#whp_conform` on ARM64EC.

Inspection of the pinned LLVM fork's
`clang/lib/Parse/ParsePragma.cpp` and Parser pragma
handler declarations found **no `conform` handler**.
This is a compiler-language parity gap across targets,
**not** evidence of an ARM64EC-specific code-generation bug.

## Water wrapper's independently confirmed dispatch hole

The GCC-style `tools/winegcc/winegcc.c` parser previously
treated unrecognized slash-prefixed `/Zc:forScope` and
`/Zc:forScope-` as positional filenames. The published
minimal correction consumes `/Zc:forScope` as the existing
standard C++ behavior (for Windows PE targets), and
**explicitly rejects** `/Zc:forScope-` because Clang
cannot implement it. The ordinary on-setting is not
forwarded as a GCC input filename. Win16 `-m16` and
non-PE host paths do not acquire this MSVC-specific mode.

The repair deliberately does **not** claim that
`#pragma conform(forScope, off)` now works inside Clang.
Implementing that MSVC lexical-scope extension would
require careful parser/Sema/lexical-scope changes,
cross-target and precompiled-header tests, and a distinct
evidence-based justification. Do not fake it by
manipulating ARM64EC calling conventions or replacing
`off` with `on` silently.

## Regression contract

`sh scripts/tests/msvc-conform-abi.sh` runs object and
scope parity tests for i686, x64, ARM64 and ARM64EC.
Supply a **freshly compiled** Water `winegcc` to
add mock-compiler forwarding and rejection tests:

```sh
sh scripts/tests/msvc-conform-abi.sh
sh scripts/tests/msvc-conform-abi.sh /path/to/built/winegcc
```

The standalone Clang 17 test was run successfully in this
round. The freshly compiled Water `winegcc` integration
was unavailable; it and Microsoft's own `cl.exe`
were **not** executed. Compiler compatibility with
Microsoft's named `conform` push/pop is **not implemented**
or implied by the test's success.

**Research follow-up:** ABI-bearing pragmas such as
`#pragma pack`, `#pragma vtordisp`, and
`#pragma pointers_to_members` deserve *separate*
ARM64EC/x64 structure-layout and C++ object-model
audits. They must not be conflated with `conform(forScope)`.

Sources:
- https://learn.microsoft.com/en-us/cpp/preprocessor/conform
- https://learn.microsoft.com/en-us/cpp/build/reference/zc-forscope-force-conformance-in-for-loop-scope
- https://learn.microsoft.com/en-us/cpp/preprocessor/pack
