# LLVM libc++ migration: PowerPC Windows ABI readiness

## Continuity (14 October 2026)

Water already has a modern, pinned LLVM libc++ 24 provider, configured
with `LIBCXX_CXX_ABI=vcruntime` and a private, static
`libwhp-libcxx.a` per validated MSVC PE architecture. It is not a
new migration. Modern provider paths in `build.sh` include x86_64,
aarch64, arm64ec, and opt-in i386 via `WATER_LIBCXX_I386=llvm`.
The old libc++ 8 tree (`_LIBCPP_VERSION 8000`) still supplies
legacy fallback for i386 by default, ARM32, and PowerPC.

Modern LLVM libc++ does **not** make every Clang target a viable
Windows PE runtime. The Water PowerPC lane uses
`powerpcle-windows` / `powerpcle-w64-mingw32` target variants.
These are historically distinct from Windows x64, ARM64, ARM64EC,
and PowerPC Linux/AIX. Mixing ELF PowerPC objects, Itanium/GNU C++
ABI libraries, or PPC host runtimes into Windows PE is forbidden.

LLVM's official libc++ supported-target matrix lists PowerPC for
AIX, not Windows PowerPC. Support for Windows libc++ with the
vcruntime C++ ABI is separate from generic PowerPC CPU lowering:
https://libcxx.llvm.org/
https://libcxx.llvm.org/DesignDocs/WindowsSupport.html

## Verified current blocker

Locally installed Clang 17 accepted these `-target` triples but
emitted the *wrong object format*:

| Clang target | Observed object |
| --- | --- |
| `powerpcle-windows` | ELF32 little-endian PowerPC |
| `powerpcle-w64-mingw32` | ELF32 little-endian PowerPC |
| `powerpc-windows-gnu` | ELF32 big-endian PowerPC |
| `powerpc-unknown-windows-msvc` | ELF32 big-endian PowerPC |

The PPC Windows-named Clang target also did not define the
`_WIN32` preprocessor macro in the tested toolchain. Therefore
neither the Windows object format nor the Windows C++ ABI may be
inferred from the name `powerpcle-windows` alone.

A minimal `dynamic_cast` C++ probe with
`-fshort-wchar -std=c++17 -fexceptions -frtti` successfully
compiled on the Linux x86_64 test host into **ELF32** PowerPC,
confirming the format defect is not merely a missing C++ syntax
feature. This does **not** establish working Windows exception
handling, RTTI, vcruntime, or importing/loading a PPC PE DLL.

The repair in `configure.ac` checks the actual object emitted
by the selected PowerPC C++ compiler using its configured
`CXXFLAGS`. It requires little-endian
`IMAGE_FILE_MACHINE_POWERPC` (`0x01f0`) or
`IMAGE_FILE_MACHINE_POWERPCFP` (`0x01f1`) in a COFF object
header, not merely a successful compiler exit. If the architecture
was explicitly requested, this rejects the invalid target before
a libc++ archive, import library or PE DLL is built. For automatic
architecture discovery, the existing `WINE_NO_PE_ERROR` mechanism
can exclude an unsupported lane rather than damaging other builds.
This **does not implement** PPC PE/COFF or its relocations.

## Modern-only/retirement mode

Water now supports `WATER_LIBCXX_LEGACY_POLICY=allow|error`
in `build.sh` and `menuconfig`.

* `allow` (**default**) preserves verified older fallback paths and
  keeps mixed architectures buildable where possible.
* `error` refuses `WATER_LIBCXX=legacy`, refuses i386 legacy
  fallback unless `WATER_LIBCXX_I386=llvm`, refuses ARM32 and
  PowerPC in selected architectures, and refuses user-supplied
  unverified replacement C++ provider overrides. That ensures
  no `_LIBCPP_VERSION 8000` fallback silently enters a
  requested *strict modern-only* build. It does **not** make
  ARM32 or PowerPC LLVM libc++ ready.
* The strict setting is included in the C++ build profile
  signature, so changing it is not treated as identical to a
  prior legacy-enabled build.
* `WATER_ARCHS_MODE=auto` enumerates all candidate PE lanes in the
  libc++ preflight. To test a modern-only set, select explicit
  `WHP_CONFIGURE_ARCHS` / `WATER_ARCHS_MODE=custom` values
  rather than expecting an unsupported PPC lane to disappear
  silently. Existing i386, x64, ARM64, and ARM64EC C++ runtime
  tests and object-format audits still apply.

Example modern-only scope *without the unsupported PowerPC lane*:

```sh
WHP_GIT_UPDATE=0 \
WATER_LIBCXX=llvm WATER_LIBCXX_LEGACY_POLICY=error \
WATER_LIBCXX_I386=llvm WHP_CONFIGURE_ARCHS=i386,x86_64,aarch64,arm64ec \
./build.sh build
```

This is a **test configuration**, not a declaration that all
four runtimes are production-ready; i386 is still experimental.

## Reproducible PowerPC audit

```sh
sh scripts/tests/libcxx-powerpc-abi.sh
python3 scripts/tests/libcxx-provider-policy.py
```

The PowerPC regression checks the legacy-selection guard in source,
compiles a 32-bit PPC C++ dynamic-cast fixture, inspects the
raw object signature, and emits `BLOCKED` for ELF or other
non-Windows formats. It reports a missing `_WIN32` macro
separately, so early preprocessor failure cannot conceal the
ELF problem. If a future toolchain emits genuine PPC COFF, it
also probes minimal PowerPC DLL linking before declaring that
format ready for further auditing. A `BLOCKED` result exits **77** (skipped/unavailable), not zero,
and must **not** be treated as a successful PPC libc++ migration.
The separate source-function test executes Water's real
`selected_libcxx_archs` and `prepare_libcxx_provider` shell functions
with the actual C++ build stubbed out. It checks every legacy
fallback, strict rejection, modern-only architecture selection and
profile-state separation. Neither test builds libc++ 24 itself.

## Remaining gates before deleting libc++ 8

1. Make Clang/LLVM produce **real Windows PPC COFF** objects and
   enable valid PE/COFF relocations in the linker.
2. Verify PowerPC import-library creation, symbol decoration,
   and the correct binary ABI (CRT, calling convention, exception
   frames, RTTI and `wchar_t` layout). Object format alone is
   insufficient.
3. Build LLVM-fork libc++ 24, libc++abi/vcruntime selection, and
   required unwind/CRT support *for that exact Windows PPC ABI*;
   validate all archive members and a live C++ consumer.
4. Rewire `powerpc_CXX_PE_CFLAGS` and `powerpc_CXX_PE_LIBS`
   with an architecture-specific modern provider, then test
   install/uninstall paths.
5. Repeat for ARM32 and i386 and verify no legacy consumers remain
   before physically removing `libs/c++`, `libs/c++abi`,
   or the older `include/msvcrt` C++ headers. Do not delete
   `libs/unwind` or `compiler-rt` as collateral cleanup.

Verification levels: local Clang 17 C++ target/object probes
**executed**; source and GitHub commits **verified**. Full
PowerPC PE toolchain/build, Windows runtime, and end-to-end
llvm-libc++ replacement **not verified**.
