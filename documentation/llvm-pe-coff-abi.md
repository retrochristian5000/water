# LLVM PE/COFF ABI audit for Water

State: **SOURCE-AUDITED + PATCH PUBLISHED; BUILD/LLD TESTS UNVERIFIED**.
Updated: 2026-10-09. Do not treat this as evidence of a successful Water
Windows build. Recover prior evidence before adding further link-time rules.

See also the [`/F` executable stack-reserve audit](llvm-msvc-f-stack-abi.md), including PE32 reserve/commit overflow guards.

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
