# LLVM libunwind migration: Water

## ARM64EC legacy unwind isolation — 15 October 2026

**Published source repair, integration build pending.** The Microsoft C++
ABI/SEH runtime in `vcruntime140` is not the LLVM/GNU Itanium unwinder.
This is directly confirmed by the pinned LLVM fork's own
`libunwind/CMakeLists.txt`, which rejects MSVC builds. The newer
`libunwind/src/Unwind-seh.cpp` still describes *SEH-based Itanium C++
exceptions*: seeing the word SEH there does **not** make it a
Microsoft C++ exception provider for ARM64EC.

The actual Water configuration had a nested library chain
`CXX_PE_LIBS -> CXXABI_PE_LIBS -> UNWIND_PE_LIBS`. The global defaults
contained `c++abi` and `unwind`, even on architectures whose C++
exceptions instead use the Microsoft runtime. Simply setting
`arm64ec_CXXABI_PE_LIBS=` and `arm64ec_UNWIND_PE_LIBS=`
was insufficient: `tools/makedep.c` intentionally discarded empty
architecture-specific overrides and returned the global provider.

**Repairs, scoped to ARM64EC:**

- `configure.ac` now publishes architecture-specific
  `CXXABI_PE_CFLAGS`, `CXXABI_PE_LIBS`, `UNWIND_PE_CFLAGS` and
  `UNWIND_PE_LIBS` settings in the generated top-level Makefile,
  explicitly blanks ARM64EC's legacy ABI/unwind provider settings,
  rejects caller-supplied nonempty ARM64EC library providers in
  these lanes, and appends `libs/c++abi` plus `libs/unwind` to
  `arm64ec_DISABLED_SUBDIRS`.
- `tools/makedep.c` honors an **explicitly empty** ARM64EC override
  for only those four settings during nested library expansion.
  Other variables and architectures keep their established
  fallback behavior. ARM64EC's ordinary `CXX_PE_LIBS` remains
  eligible to link libc++ and the Microsoft `msvcp140`/
  `vcruntime140` providers; no Itanium `_Unwind_*`
  dependency is substituted for Microsoft `__CxxFrameHandler3`.
- The bundled LLVM-8-era `libs/unwind` and `libs/c++abi`
  source trees are **not deleted**. Eligible GNU PE targets
  continue to use their existing configurations until a
  source-version-matched modern LLVM libunwind is built,
  checked with `check-unwind-provider.sh`, and tested
  against a matching Itanium libc++abi consumer.

**Direct COFF evidence:** Clang/LLVM 17 object-generation probes for
x64, native Windows ARM64 and ARM64EC C++ throw/catch functions
used `__CxxFrameHandler3`, `_CxxThrowException`, and
`.pdata`/`.xdata`, not `_Unwind_*`. ARM64EC additionally
emitted `$h` C++ mangling and undefined
`__os_arm64x_dispatch_ret`/related hybrid-runtime helpers.
This proves the expected static *object contract*, **not**
runtime exception propagation through an x64/ARM64EC thunk.
Never fill in those helpers with LLVM libunwind or placeholders.

**Regression:** `sh scripts/tests/arm64ec-unwind-isolation.sh`
checks the compiler-level Windows C++ exception artifacts;
optionally supply the **configured Water Makefile path** to
also check empty provider variables, disabled external
libraries and nested ARM64EC C++ link dependencies:

```sh
sh scripts/tests/arm64ec-unwind-isolation.sh /path/to/build/Makefile
```

Direct Clang/LLVM 17 operations were executed successfully.
A freshly configured Water build and an ARM64EC Windows runtime
were not available: neither generated Makefile integration nor
runtime unwinding should be reported as passed.

Source references:
- https://clang.llvm.org/docs/Toolchain.html
- https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi

---


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
