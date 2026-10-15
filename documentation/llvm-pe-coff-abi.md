## ARM64EC import-library ABI audit (15 October 2026)

**Outcome: repair the library producer; retain separate runtime/thunk work.**
A standard COFF `.lib` is an archive of object/import members, not one
machine-coded PE image. Microsoft's ARM64EC documentation identifies
`0xA641` for ARM64EC **object/import members**, while linked hybrid
DLL/EXE images use an x64-compatible or native ARM64 PE machine value
as appropriate. A hybrid `.lib` may legitimately contain distinct
ARM64EC and ARM64 records; do not classify it by an arbitrary
first member or flatten it into pure ARM64.

Before this audit, `tools/winebuild/import.c` routed ARM64EC through
the generic `dlltool` import-library generator by default and
failed if that tool was absent. `--without-dlltool` instead entered
the internal Win32 thunk emitter, which asserts for CPU_ARM64EC.
Also, `parse_def_file` filled only the target exports, unlike the
`.spec` parser, leaving the native export view empty for
`-marm64x` with `.def` input. Appending members through a generic
archiver did not explicitly rebuild the specialized EC symbol index.

**New route:** ARM64EC `winebuild --implib` always uses `lld-link
/lib /machine:arm64ec /def:<generated.def> /out:<library>`.
With `-marm64x`, it adds
`/defarm64native:<generated-native.def>`, keeping distinct ARM64
exports. Extra object members are appended by `lld-link /lib`
itself, with the existing archive as input and output, so its COFF
index gets rebuilt rather than treated like an ordinary `ar` index.
The `--without-dlltool` option no longer falls through to the
ARM64EC `assert(0)` internal thunk path. The existing non-ARM64EC,
PowerPC, and Win16 paths are unchanged. `.delay.a` is explicitly
rejected for ARM64EC: MSVC `/lib` output is *not* a substitute for
GNU `dlltool -y` delayed-load glue.

**Standalone evidence on LLVM/LLD 17:** `lld-link /lib` emitted
an EC-specific `/<ECSYMBOLS>/` archive index. EC short-import members
used machine `0xA641`; the code import used a `#` name and retained
its unadorned exported DLL name, while data imports kept the ordinary
name. With `/defarm64native`, the same archive also contained ARM64
`0xAA64` short-import members. A second `/lib` pass accepted an
additional ARM64EC object and retained the EC index and both machine
views. Data-import consumers assembled for both ARM64EC and native
ARM64 linked successfully against the hybrid library without an
ARM64EC runtime. This validates archive structure and **data-import**
lookup, not Windows runtime loading or function-call thunks.

**Residual evidence:** a different ARM64EC function consumer can
still require `__os_arm64x_dispatch_ret` from a hybrid call runtime.
A no-runtime function-import probe with the local LLD 17 binary also
failed to resolve `__imp_aux_...` in one configuration, although
the EC index contained the corresponding entry. The present
archive patch does not claim to repair that consumer-side issue;
repeat it with the pinned LLVM fork and a correct ARM64EC CRT to
distinguish a linker defect from missing helper libraries.

**Regression:** `sh scripts/tests/arm64ec-import-lib.sh` checks
archive machine records, EC index, native view, extra member, and
cross-ABI data-import lookup with stand-alone Clang/LLD tools.
`sh scripts/tests/arm64ec-import-lib.sh /path/to/winebuild`
additionally exercises the real Water producer and
`--without-dlltool -marm64x` path. The direct Clang/LLD test
operations were performed successfully in this round, but a
newly built Water `winebuild` was not available to execute the
integration cases. Treat that part as **unverified**.

References:
- https://learn.microsoft.com/en-us/windows/arm/arm64ec
- https://learn.microsoft.com/en-us/windows/arm/arm64x-build
- https://www.llvm.org/doxygen/COFFImportFile_8cpp_source.html

## ARM64EC dllimport local-import reachability (15 October 2026)

**Confirmed failure, targeted fix, and pinned LLVM submodule.**
Microsoft documents `__declspec(dllimport)` as a compiler-directed
IAT-indirect access, while ARM64EC requires hybrid entry/exit thunks
and extra `__imp_aux_` import names. This does *not* mean that
80286/Win16 symbols can be linked into ARM64EC COFF images.

A standalone Clang 17 / LLD 17 test reproduced an LLD/MS LINK
behavior mismatch with x64 COFF objects: a target function placed
in its own discardable COMDAT, referenced only through an
`__imp_target` local import, disappears under `/OPT:REF`.
The emitted local-import pointer then contains the image base,
not a function address. The **same objects** linked with
`/OPT:NOREF` retained the function and a valid pointer. Without
`-ffunction-sections` the target was a non-COMDAT section and
the bug did not reproduce; recording this negative control is
important. LLD also emitted the usual locally defined symbol
imported warning (LNK4217). This agrees with LLVM issue #220370,
which reports that MSVC LINK keeps the local target under
`/OPT:REF`, but Microsoft LINK was not installed for this probe.

**Actual repair in retrochristian5000/LLVM:** LLD's
`LocalImportChunk` and `DefinedLocalImport` now expose the
locally resolved target. `lld/COFF/MarkLive.cpp` follows
`DefinedLocalImport` references to that target when marking
live chunks, retaining locally imported COMDAT definitions.
This is a reachability fix; it does **not** disable section GC,
fabricate imports, or alter ARM64EC instruction lowering.
`lld/test/COFF/locally-imported-gc.test` adds both x64
`__imp_target` and ARM64EC `__imp_aux_target` positive cases,
plus an unrelated dead COMDAT control. Water's LLVM gitlink now
pins fork commit `65c6fbb6ae224813abe4c4c56bd583f9d1388b5f`,
advanced from `ef6834df8e0dc4eaf3ebf99eb598448583327a3a`.
The 11-commit fast-forward also includes existing Apple arm64e
CI/test changes; **those unrelated changes are not being claimed
as part of the import fix**.

**Compiler-side negative evidence:** a separate Clang 17
`-target arm64ec-windows-msvc` object emitting a
`__declspec(dllimport)` function call and variable access
contained undefined `__imp_whp_func` and `__imp_whp_data`
symbols. The equivalent x64 and ordinary ARM64 objects emitted
the same `__imp_` forms. These observations alone do **not**
establish a Clang mangling/codegen defect. The LLVM fork
`AArch64MCInstLower.cpp` already implements ARM64EC's
`__imp_aux_` selection; do not replace it with generic ARM64
import logic. Water previously repaired `_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS`
for static libc++ archives; that separate producer/consumer
annotation error is not evidence that Clang's `dllimport` lowering
is inherently wrong.

**Remaining distinct failure:** LLVM issue #220371 reports
distributed ThinLTO preserving `dllimport` on symbols resolved
locally when in-process ThinLTO clears it. It may cause
indirect-call overhead and local-import warnings and, combined
with the LLD GC bug, incorrect images. The LLD reachability
fix is a guard against the crash class, **not a verified repair
of the distributed ThinLTO code-generation discrepancy**.

**Validation state:** LLVM 17 reproduction and its negative
controls were executed; the three-file LLD patch and the dual
architecture lit regression are committed on the fork, and
Water's submodule pin is published. The **modified fork has
not been rebuilt or had llvm-lit executed** in this round.
Use the fork's built LLVM tools:

```sh
llvm-lit -sv lld/test/COFF/locally-imported-gc.test
```

Do not claim runtime ARM64EC loading, full Water DLL builds,
Microsoft LINK binary parity, or corrected distributed ThinLTO
from this narrow change.

References:
- https://learn.microsoft.com/en-us/cpp/build/importing-function-calls-using-declspec-dllimport
- https://learn.microsoft.com/en-us/windows/arm/arm64ec-abi
- https://github.com/llvm/llvm-project/issues/220370
- https://github.com/llvm/llvm-project/issues/220371

# LLVM PE/COFF ABI audit for Water

State: **SOURCE-AUDITED + PATCH PUBLISHED; BUILD/LLD TESTS UNVERIFIED**.
Updated: 2026-10-09. Do not treat this as evidence of a successful Water
Windows build. Recover prior evidence before adding further link-time rules.

See also the [`/F` executable stack-reserve audit](llvm-msvc-f-stack-abi.md), including PE32 reserve/commit overflow guards.

See also [MSVC `/vlen` and AVX10 target-feature continuity](llvm-msvc-vlen-abi.md): vector-width preferences must not change the enabled instruction-set features.

## Architecture boundaries

| Water output or build stage | Architecture | File format | Critical constraints |
| --- | --- | --- | --- |
| Water bootstrap running on macOS Apple Silicon | aarch64-apple-darwin (or arm64e if explicitly supported) | Mach-O host | Host linker is Apple's system linker by default: `WATER_LLVM_LINKER=system`. Host pointer authentication and arm64e rules are **not PE** rules. |
| Windows 95/98/Me executables and Win32 DLLs | i386, Win32 ABI | PE32/COFF (`0x10b`, machine `0x014c`) | Keep x86 stdcall/cdecl symbol decoration, PE32 thunk/layout and 32-bit import conventions separate. |
| Windows x64 | x86-64, Win64 ABI | PE32+ (`0x20b`, machine `0x8664`) | x64 calling convention, function-table exception metadata, 64-bit image pointers. |
| Windows on ARM64 | AArch64, Win64 ARM64 ABI | PE32+ (`0x20b`, machine `0xaa64`) | AArch64 relocations (ADRP/ADD/LD), aligned code/data, .pdata/.xdata. |
| ARM64EC/ARM64X | Hybrid Windows x64/ARM64 ABI | Hybrid PE32+ | Hybrid CHPE metadata, import thunks, IAT layouts; not interchangeable with native ARM64. |

Water's LLVM bootstrap enables COFF and MinGW LLD backends. On a
Mac host, `WATER_LLVM_LINKER` governs host linking and does not by
itself imply that an x86 Windows guest was linked by Mach-O LLD.

Microsoft PE format reference:
<https://learn.microsoft.com/en-us/windows/win32/debug/pe-format>

## Verified LLVM source defects / guards

### Previous fix (already in Water)

LLVM commit `b9a24b4d1f390d9eab7a345c6b04d70f0e3288a1`:
`lld/COFF/Driver.cpp` rejects `/align:<4096` on ARM64-family PE
images. AArch64 ADRP relocation encoding depends on fixed 4 KiB
page offsets. The `arm64-section-align.s` test covers inferred
and explicit target machine values. Upstream issue:
<https://github.com/llvm/llvm-project/issues/172660>.

### New fix

LLVM commit `75d3f46234c37ddec4472fedf9ec876aff2ad36f`:
the ARM64-family guard now rejects
`FileAlignment > SectionAlignment` (for example,
`/filealign:8192 /align:4096`). Microsoft PE requires section
alignment greater than or equal to file alignment. The regression
test covers ARM64, ARM64EC, ARM64X, inferred ARM64, and a positive
case where both alignments equal 8192.

Intentionally not changed: existing x86 and special driver/EFI
subpage-alignment semantics. There is **no evidence** this guard
directly fixes an x86 Windows 98 Water executable.

### Other issue checked — do not duplicate

Upstream LLVM issue #218899: AArch64 COFF `extern_weak dso_local`
symbol lowering can emit a scaled `PAGEOFFSET_12L` and fail with a
misleading `misaligned ldr/str offset`. The checked LLVM fork
already contains `AArch64ExpandPseudoInsts.cpp` handling for
under-aligned external weak symbol addresses and
`llvm/test/CodeGen/AArch64/coff-loadgot-weak-extern.ll`.
`windows-extern-weak.ll` separately checks a direct-call path.
No repeat patch was added.
<https://github.com/llvm/llvm-project/issues/218899>

## Verification and next diagnostics

1. Build the pinned LLVM revision on the actual macOS/arm64 host.
2. Run `llvm-lit -sv lld/test/COFF/arm64-section-align.s` using
   the matching built `llvm-mc`, `llvm-readobj`, and `lld-link`
   tools; check the new invalid and valid header cases.
3. Build Water's **i386 Win9x** binaries separately; inspect each
   output using `llvm-readobj --file-headers --coff-imports
   --coff-exports file.exe` and inspect its actual PE32/machine
   values and unresolved imports. Adapt command options to the
   built tool's actual support rather than assume success.
4. Independently run x64, ARM64 and ARM64EC fixtures (when configured);
   check machine type, PE optional-header magic, section/file
   alignment, and import/export ABI as appropriate.
5. Distinguish a compiler diagnostic (C++ source), link diagnostic
   (COFF relocations), and runtime crash (loader/ABI) before assigning
   a defect to LLVM. Avoid altering Water guest personalities
   on evidence from macOS host-link failures.
6. The observed Water Actions pipeline had CodeQL jobs rather than
   a real LLVM/PE build at the time of this audit. The observed LLVM
   workflow failed during `Set up job`, **before compilation**.
   These observations are not validation of the new code.

Preserve native object and binary evidence, negative tests, and
ABI-specific assumptions when extending this ledger.

See also the [Microsoft C++ event extension ABI audit](llvm-ms-events-abi.md) for missing `__hook`/`__unhook`/`__event`/`__raise` semantics and fail-closed attributed-event diagnostics.

## MSVC compiler flags: /GS is not /Gs

See [Microsoft buffer-security and stack-probe ABI audit](llvm-msvc-gs-abi.md).
These options differ by case and by purpose; verified LLVM fork fixes
`/Gs` target-specific threshold handling,
while `/GS` controls stack-security cookies. Do not infer successful
Water CRT initialization from compiler emission of helper relocations.

## Microsoft C++ native `__interface` — LLVM/Clang code-generation correction

10 October 2026 source audit (updated 11 October): Microsoft documents
`__interface` as a **native C++ COM-style extension**. With
`-fms-extensions`, Clang already recognizes the keyword:
`clang/include/clang/Basic/TokenKinds.def` contains
`KEYWORD(__interface, KEYMS)`; `ParseDeclCXX.cpp` and
`SemaDeclCXX.cpp` enforce interface rules; existing tests include
`clang/test/SemaCXX/ms-interface.cpp` and
`clang/test/CodeGenCXX/microsoft-interface.cpp`.
Do **not** add a second lexer/parser keyword or mistake C++/CLI
`interface class` or C++20 `/interface` modules for this construct.

**Verified source gap:** Microsoft's native `__interface` implicitly
requests `__declspec(novtable)`. In the pinned LLVM parent
`066483da1f0a8026263d99ec019bb5161ef0faa1`, the Microsoft
ABI's `doStructorsInitializeVPtrs` checked only an explicit
`MSNoVTableAttr`; `__interface` alone was not excluded. A
Windows-targeted Clang IR probe with a concrete derived object
emitted an unnecessary `IContract` base-interface vftable and
vfptr store, while an explicitly marked
`struct __declspec(novtable) IExplicit` suppressed both.
This is **code-generation behavior**, not a missing parser keyword.

**Published fork fix:** LLVM
[commit 5acacdbe342b10b701ed64f2cd1de4ae41e6271a](https://github.com/retrochristian5000/LLVM/commit/5acacdbe342b10b701ed64f2cd1de4ae41e6271a)
makes MicrosoftCXXABI skip vfptr initialization for both explicit
`novtable` and native `isInterface()` classes. The new
`clang/test/CodeGenCXX/ms-interface-implicit-novtable.cpp`
requires absence of the interface vftable while preserving concrete
derived vftables, on Windows i386/x64/ARM64 for C++11 and C++26.
Water's LLVM submodule advances **only** from the exact previously
pinned fork head to this commit.

Regression command after building the pinned LLVM toolchain:

```sh
llvm-lit -sv clang/test/CodeGenCXX/ms-interface-implicit-novtable.cpp
```

**Verification boundary:** system Clang 17 reproduced the
unpatched vtable difference; source/branch linkage is verified.
The modified fork was **not rebuilt here** and its six
`llvm-lit` cases have not yet been executed. This fix applies
to the Microsoft C++ ABI and does **not** change macOS arm64e
vtable semantics. Clang documents `__is_interface_class`
returning `false` for native `__interface`: Microsoft documents
that intrinsic for platform `interface class`, so do not
assume changing its result would fix the native COM ABI.

Sources:
- https://learn.microsoft.com/en-us/cpp/cpp/interface?view=msvc-170
- https://learn.microsoft.com/en-us/cpp/cpp/novtable?view=msvc-170
- https://clang.llvm.org/docs/LanguageExtensions.html
- https://learn.microsoft.com/en-us/cpp/extensions/compiler-support-for-type-traits-cpp-component-extensions?view=msvc-170
