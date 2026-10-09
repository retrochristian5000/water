# LLVM PE/COFF ABI audit for Water

State: **SOURCE-AUDITED + PATCH PUBLISHED; BUILD/LLD TESTS UNVERIFIED**.
Updated: 2026-10-09. Do not treat this as evidence of a successful Water
Windows build. Recover prior evidence before adding further link-time rules.

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
