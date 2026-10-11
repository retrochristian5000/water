# Microsoft `/vlen` vector-width optimization and LLVM target ABI audit

Status (11 October 2026): **SOURCE FIX PUBLISHED AND PINNED; PATCHED COMPILER BUILD, LIT AND EXECUTION TESTS NOT YET RUN**.

## Scope and chronology

Microsoft introduced the clang-cl-style `/vlen` vector preference in Visual Studio 2022 **17.13**, not Windows 95/98-era MSVC. Microsoft specifies this option for **x86 and x64**, not ARM64 SVE, and distinguishes it from `/arch` (minimum/selected ISA). The syntax is bare `/vlen`, `/vlen=256`, or `/vlen=512`; explicit values override the default vector preference for `/arch:AVX512`, `/arch:AVX10.1`, and `/arch:AVX10.2`. For example, `/arch:AVX512 /vlen=256` requests 256-bit autovectorization while preserving AVX-512 availability. `/arch:AVX2 /vlen=512` warns and uses AVX2's default.

### Ownership boundary

| Compiler surface | Established handling | Verification relevance |
| --- | --- | --- |
| `clang/include/clang/Options/Options.td` | `/vlen`, `/vlen=256`, `/vlen=512` already recognized | Not a missing keyword/option |
| `clang/lib/Driver/ToolChains/Clang.cpp` | Translates preferences into `-mprefer-vector-width=256/512`; selects `/arch:AVX10.1`/`AVX10.2` target features | Real bug was **feature selection conditional on absence of `/vlen`** |
| `clang/lib/Driver/ToolChains/Arch/X86.cpp` | Both AVX10 variants map to the `sapphirerapids` CPU baseline | AVX10.2 needs additional `+avx10.2` feature independently of baseline |
| `llvm/lib/Target/X86/X86.td` | Exposes `avx10.1` and `avx10.2` subtarget features | Distinct ISA selection from preferred vector width |
| `llvm/lib/Target/X86/X86TargetTransformInfo.cpp` | Vectorization width depends on subtarget ISA **and** preference | Width does not grant unavailable ISA instructions |
| `clang/test/Driver/cl-x86-flags.c` | Existing coverage of widths and incompatibility diagnostics | Earlier checks could pass while `+avx10.2` was silently absent |

## Verified source defect and published fix

At fork parent `481c053c9f7df10cb292a33d0bdb8bd96d9069bf`, `AddClangCLArgs()` had an `if (/vlen is present)` branch for width, and an `else` branch that **also** appended `-target-feature +avx10.2` and `-target-feature -amx-tile` for `/arch:AVX10.x`. This meant that specifying `/arch:AVX10.2 /vlen=512`, `/vlen=256`, or bare `/vlen` **suppressed** the AVX10.2 feature, while `/arch:AVX10.2` alone retained it. The selected CPU was `sapphirerapids`, so leaving out that extra feature can change available instructions.

LLVM fork [commit 3ca674b](https://github.com/retrochristian5000/LLVM/commit/3ca674b360059614fc3572fa81bdf25ea9b9f5d5) moves both AVX10-specific feature emissions **outside** the `/vlen` conditional, preserving the existing preferred-width selection. No separate parser, vector layout, x64 calling-convention, or ARM64 path is invented.

New `clang/test/Driver/cl-vlen-avx10.c` has **10** driver invocations for Windows i386 and x64: three `/vlen` forms and no-vlen control on AVX10.2; AVX10.1 negative feature controls; AVX512 width preference; and AVX2 invalid-width diagnostics. It checks `-mprefer-vector-width` and `-target-feature` **together** rather than accepting width-only evidence.

## ABI risks that remain hypotheses until binary/runtime tests

- The `/vlen` option is an **optimization preference**; it does not intrinsically change Windows x64 or x86 calling conventions, externally declared SIMD type sizes, PE/COFF header structure, or `__vectorcall` layout. Treat any ABI change as an independently demonstrated defect rather than an assumption.
- Check x86 vs x64 register-passing, callee-saved YMM/ZMM fragments, stack alignment, `__vectorcall` parameter/return rules and unwind metadata on actual compiler outputs. Cross-DLL interfaces may be particularly sensitive to explicit vector types, but `/vlen` alone does not rewrite their signatures.
- Check that AVX10.2-only instructions really appear only for matching supported CPUs, and that `/vlen=256` does not accidentally generate unsupported 512-bit operations on a 256-only feature set. Mere `-###` command printing is not sufficient.
- Prevent retrojection: Visual Studio 2022 17.13 and later AVX10 settings are modern compiler tooling, not historical Win9x requirements. Water's x86 Win9x, x64 Windows, and macOS arm64 bootstrap host are separate lanes. `/vlen` is **not** the ARM64 SVE vector-length control.

## Reproducible follow-up

After building Water's pinned fork (not independently built in this round):

```sh
llvm-lit -sv clang/test/Driver/cl-vlen-avx10.c
llvm-lit -sv clang/test/Driver/cl-x86-flags.c
```

Then compile vectorized x86/x64 Windows objects with `/arch:AVX10.2` + all three `/vlen` forms; compare the compiler's `-###` output, emitted instruction encodings, explicit SIMD argument layouts, and unwind tables against MSVC on the **same compiler version and target CPU**. Reject unsupported `AVX2 /vlen=512` cases rather than guessing a feature promotion. These compiled-binary and runtime tests remain **open**.

## Sources

- Microsoft `/vlen` reference: https://learn.microsoft.com/en-us/cpp/build/reference/vlen?view=msvc-170
- Microsoft `/arch` x64: https://learn.microsoft.com/en-us/cpp/build/reference/arch-x64?view=msvc-170
- Clang option manual: https://clang.llvm.org/docs/UsersManual.html
