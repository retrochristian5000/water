# Microsoft /arch: ARM64EC instruction-set selection audit

**15 October 2026 — World History Project / Water ABI ledger.**
State: **SOURCE REPAIRS PUBLISHED; LLVM AND WATER INTEGRATION BUILDS NOT YET VERIFIED**.
No platform reset, Win16 relabeling or architecture overmerging.

## Microsoft's documented contract

Microsoft `/arch` controls the ISA instructions the compiler may
emit for a **specific target architecture**. It is not the ABI
selector, not `LIB /MACHINE`, not the compiler target triple, and
not Apple's Mach-O `-arch` spelling.

| Target | Microsoft /arch examples | What it selects |
| --- | --- | --- |
| x86 PE32 | `IA32`, `SSE2`, `AVX2`, `AVX10.2` | IA-32 instruction and vector extensions |
| x64 PE32+ | `SSE2`, `AVX2`, `AVX512`, `AVX10.2` | x86-64 instruction and vector extensions |
| ARM64 (AArch64) | `armv8.0`, `armv8.2+lse`, `armv9.4` | A-Profile ISA baseline and optional feature modifiers |
| ARM64EC (hybrid ABI) | AArch64 **ISA**, x64-compatible **software ABI** | Keep hybrid calling-convention, registers, decorated symbols, COFF metadata and CRT thunks distinct from native ARM64 |

Microsoft's ARM64 `/arch` page gives ARMv8.0–8.9 and
ARMv9.0–9.4 with optional `+feature` modifiers as examples.
It documents that `/arch:armv8.1` can enable LSE atomic
instructions when supported. The cited Microsoft page explicitly
says **ARM64 compiler**, not every ARM64EC compiler version; the
ARM64EC mapping is a Water/LLVM porting decision grounded in the
same underlying AArch64 ISA, not proof of identical Microsoft
command-line behavior in all MSVC toolchains.

**Protected-mode boundary:** `/arch:IA32` is not a 286 protected-mode
profile. It controls enhancements of modern IA-32 code generation;
Win16 MZ/NE segmentation, descriptors, 16-bit far calls and
286-specific behavior must remain outside PE/ARM64EC builds.
Darwin `-arch arm64` / arm64e refers to the Mach-O architecture/ABI
and must not be confused with Microsoft `/arch:`.

## Reproduction on installed Clang/LLD 17

`clang-cl --target=aarch64-pc-windows-msvc
/arch:armv8.2 /c ... -###` and the ARM64EC equivalent
both reported **argument unused**, leaving the default
AArch64 ISA level. This is a concrete missing translation
relative to Microsoft's documented ARM64 option.

The equivalent Clang GCC-style `-march=armv8.2-a+lse`
did activate `+v8.2a` and `+lse` under *both* ARM64
and ARM64EC. With an atomic fetch-add fixture:
- ARMv8-A baseline generated `ldaxr`/`stlxr` retry code;
- ARMv8.2-A+LSE generated `ldaddal`;
- ARM64EC preserved its `#add` symbol and generated hybrid entry
  thunk while using the newer atomic instruction.

That is direct evidence of ISA selection **without** silently
replacing the ARM64EC calling ABI with classic ARM64. It is
not a completed runtime test on Windows-on-Arm, and
does not establish all Armv9 extensions are available on a
specific machine.

A fork-source-only risk was also found in
`clang/lib/Driver/ToolChains/Clang.cpp`: previous custom
`/arch:AVX10.1`/`AVX10.2` and `/vlen` option handling
emitted x86 target features and vector-width hints without
checking the target architecture. Those flags could therefore
reach an ARM64EC cc1 command line even though the x86
`/arch` family has no meaning there. The installed stock
Clang 17 lacks the fork's custom AVX10 path; this case
is **source-proven risk**, not a runtime-reproduced
crash in the fork.

## Implemented repairs

**LLVM fork** (on `main`):

- `clang/lib/Driver/ToolChains/Arch/AArch64.cpp`
  now accepts Microsoft's `/arch:armv8.0`–`armv8.9`
  and `/arch:armv9.0`–`armv9.4` for Windows
  ARM64/ARM64EC, passing canonical `armv8-a`,
  `armv8.2-a+lse`, `armv9-a`, etc. to the existing
  AArch64 architecture/extension parser. It validates
  base versions instead of accepting unknown values.
  Explicit Clang `-march=` and `-mcpu=` remain authoritative.
- `clang/lib/Driver/ToolChains/Clang.cpp` limits
  the custom AVX10 /vlen feature logic to x86/x64.
- `clang/test/Driver/cl-arch-arm64ec.c` checks
  target-feature output for native ARM64 and ARM64EC,
  invalid architecture values, feature leaks and x64 AVX10
  nonregression.

**Water `winegcc` GCC-style wrapper** (on `master`):
- Formerly `/arch:armv8.2` entered the positional file
  list, since only dash-prefixed options were recognized.
- `/arch:<ARM ISA>` is now explicitly recognized
  and translated to `-march=<LLVM AArch64 spelling>`
  for Microsoft Windows ARM64/ARM64EC PE targets only,
  with uppercase normalization and rejection of
  unsupported versions and cross-architecture use.
- `-arch` (Apple), `-march` (Clang/GCC) and the
  existing x86/-m16 route remain distinct.
- `tools/winegcc/tests/msvc-arch.sh` checks forwarding
  and negative target-profile guards using a mock compiler.

**Integration pin:** Water advances the existing
`toolchains/llvm-project` gitlink from
`65c6fbb6ae224813abe4c4c56bd583f9d1388b5f` to
`6838a5f3c5f57ae3b7d5750dbeeff2e820bf4374`
via an 3-commit fast-forward. No new branch was created.
The changes do not touch lib-machines, Win16 relay code,
ARM64EC PAC metadata, or the missing
`__os_arm64x_dispatch_ret` runtime dependency.

## Verification / outstanding work

Direct Clang 17 tests of the pre-fix `/arch` behavior
and post-translation **equivalent** `-march` target-feature
and atomic assembly outputs were run.
**Neither the modified LLVM fork nor a freshly built Water
`winegcc` was compiled or executed in this round.**
The committed driver lit and wrapper mock tests must
be run against freshly compiled binaries:

```sh
llvm-lit -sv clang/test/Driver/cl-arch-arm64ec.c
sh tools/winegcc/tests/msvc-arch.sh /path/to/built/winegcc
```

Follow-up: verify AArch64 ISA feature macros and exact MSVC
ARM64EC compatibility for mixed `/arch` and `-mcpu` selections;
probe feature-specific compiler intrinsics and LSE runtime
capability checks. Do not assume host Apple arm64e PAC rules
apply to Windows ARM64EC.

## References

- Microsoft /arch (ARM64):
  https://learn.microsoft.com/en-us/cpp/build/reference/arch-arm64
- Microsoft /arch (x86):
  https://learn.microsoft.com/en-us/cpp/build/reference/arch-x86
- Microsoft /arch (x64):
  https://learn.microsoft.com/en-us/cpp/build/reference/arch-x64
- Microsoft ARM64EC ABI:
  https://learn.microsoft.com/en-us/cpp/build/arm64ec-windows-abi-conventions
