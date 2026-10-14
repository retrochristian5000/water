# Winemaker: Windows PE guest ABI preservation

## Continuity and evidence (14 October 2026)

Winemaker is an upstream-style **Winelib** project converter. Before this
repair, `tools/winemaker/winemaker` always generated `winegcc` and
`wineg++` invocations without a guest target triple and DLL targets named
`<name>.dll.so`. It also injected the obsolete `-mno-cygwin` option.
This is legitimate historical Winelib infrastructure but is **not** a
reliable build plan for a native PE32+ `aarch64-windows` DLL. Merely
adding `-m64` cannot select the ARM64 Windows ABI. The generator is the
owning component of the missing target propagation; do not mask it with
a linker-only workaround.

Earlier Water work in `tools/winegcc/winegcc.c` already distinguishes
`PLATFORM_WINDOWS`, `PLATFORM_MINGW` and ARM64EC. Previous
`winegcc` driver and alignment repairs are retained. Winemaker's
cross-target opt-in is designed to feed those existing paths, not
replace them or rewrite LLD.

## Added behavior

For native Windows PE Makefiles, invoke Winemaker with an explicit
Windows target triple:

```sh
perl tools/winemaker/winemaker --nosource-fix \
  --dll --single-target sample --target=aarch64-windows /path/to/source
```

The generated `WINEMAKER_TARGET` variable passes
`--target=aarch64-windows` to both `winegcc` and `wineg++` on
**compile and link** commands, and to `wrc` for resources. DLL
Makefile targets are `sample.dll`, not `sample.dll.so`.
Single-target PE DLLs now add `-shared` plus the selected
`.def` or `.spec` file dependency and link input; an executable
with a `.dll` suffix does not count as a DLL. The old
`-mno-cygwin` flag is not added in explicit Windows PE mode.

`--target=aarch64-w64-mingw32` remains a **different** GNU Windows
target, not an alias for the MSVC-ABI `aarch64-windows`.
`arm64ec-*-windows` is a separate hybrid ABI. A Windows 98 FE binary
remains **i386 PE32**, not ARM64 PE32+. The macOS host's
`aarch64-apple-darwin` or `arm64e` Mach-O details must not
leak into a guest Windows ABI.

Without `--target`, Winemaker retains the Winelib `.dll.so` target
convention and empty `WINEMAKER_TARGET`. Mixing the old `--wine32`
pointer-size option with an explicit target is rejected rather than
allowing an i386 switch to contaminate the ARM64 selection.
A non-Windows target triple is likewise rejected in this PE mode.

## Regression and verification levels

```sh
sh tools/winemaker/tests/pe-target.sh tools/winemaker/winemaker
```

The regression generates a PE Winemaker Makefile with separate C/C++
sources, a legacy Winelib Makefile, and a GNU Windows target variant.
It inspects native `.dll` vs Winelib `.dll.so` rules, correct
propagation of compiler/resource/link target options, `-shared`
and `.spec` metadata, absence of old `-mno-cygwin` in PE mode,
and rejection of conflicting target options. It uses `make -n`,
not an actual cross-compiler or Windows loader.

**Source-audited / published:** yes. **Generated-Makefile regression
executed:** not yet verified. **Actual ARM64 DLL build:** not verified.
**Windows ARM64 load and imported-symbol compatibility:** not verified.

Before claiming ABI repair is complete, run the regression above, then
build one real `aarch64-windows` DLL through the generated Makefile
with an actual ARM64 toolchain. Inspect with `llvm-readobj`:
COFF `Machine=IMAGE_FILE_MACHINE_ARM64 (0xAA64)`, PE32+ optional
header, nonzero DLL characteristics, entry point, exported names,
import library architecture, section/file alignments and relocations.
Compare `arm64ec-*-windows` and GNU Windows builds separately.
Check producer and consumer object architectures across `.spec.o`,
C/C++ objects and `.a`/import libraries; do not infer ABI
compatibility from successful linkage alone.

## Remaining hazards and guards

* Imported historical Visual Studio projects may contain hardcoded x86
  settings such as `-D_M_IX86`, `-mrtd` or
  `-fpack-struct`. Those require per-project scrutiny; Winemaker
  does not currently translate them to an ARM64 calling convention.
* A `.def` or `.spec` file must be valid for the requested DLL.
  Explicit PE mode does not invent missing exports or COM layouts.
* `AR` still selects the host archiver in generated Makefiles.
  For ARM64 static libraries, verify it preserves COFF object members
  or override it with an appropriate `llvm-ar` installation.
* Linker defaults, CRT selection, and actual `wrc` resource output
  remain under their owning components; don't add Mach-O flags or
  `/DRIVER` to make a DLL look loadable.
* Keep Windows 9x and modern ARM64 Windows app targeting separate.

