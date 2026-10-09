# LLVM libunwind migration: Water

## Verified source boundary

Water's old `libs/unwind` contains LLVM-8-era GNU/Itanium exception
unwinding code. The pinned LLVM fork carries libunwind 24 source at
`toolchains/llvm-project/libunwind`, but Water's modern PE libc++ uses
`LIBCXX_CXX_ABI=vcruntime` and the Windows **Microsoft C++ ABI**.
LLVM's libunwind CMake deliberately refuses MSVC targets: substituting
this library for `vcruntime140` on Windows x64/AArch64/ARM64EC is not
an ABI-compatible repair.

The historical Water `libs/unwind/src/config.h` unconditionally enabled
GCC frame-registration APIs on x86; LLVM's `LIBUNWIND_ENABLE_FRAME_APIS`
option now defaults to OFF. A GNU x86 replacement needs that option ON,
and callers must still agree on SEH/SJLJ/EHABI exception models.

## Candidate acceptance test

`scripts/check-unwind-provider.sh ARCHIVE TARGET LLVM_AR LLVM_NM [seh|sjlj]`
checks each archive member's COFF machine (including BigObj), duplicate
names, exported `_Unwind_*` entry points, and the older x86 frame APIs.
Unsupported or Microsoft-ABI targets are rejected. The checker does **not**
prove that the archive came from LLVM 24, that exceptions actually work,
or that the chosen exception model matches consumer code.

Run `scripts/tests/unwind-provider.sh` for isolated positive/negative
COFF fixture tests (requires Clang, llvm-ar and llvm-nm).

LLVM's `runtimes` CMake build is the correct future producer with
`LLVM_ENABLE_RUNTIMES=libunwind`, a static-only build and matching
compiler/target flags. On x86 GNU use
`LIBUNWIND_ENABLE_FRAME_APIS=ON`. Do not build the runtime with an
MSVC triple or infer its ABI from the macOS arm64 host.

## Retirement gate

Build and audit the LLVM archive for each active **GNU PE** target, then
wire per-architecture `UNWIND_PE_CFLAGS` and `UNWIND_PE_LIBS` overrides.
Test throw, catch, rethrow, nested exception frames, and cross-module
unwinding with the matching libc++abi consumer. Only after passing those
tests may `libs/unwind` be removed from configure and the source tree.
The old source is intentionally retained pending those tests.

See also https://clang.llvm.org/docs/Toolchain.html.
