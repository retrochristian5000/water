# WineGCC build path audit (October 9, 2026)

## Confirmed and repaired

Water `tools/winegcc/winegcc.c` previously treated every single-dash
`-i...` option as requiring a *separate* operand. This swallowed the
next argument after Clang's self-contained `-integrated-as`, or after
joined include/sysroot options such as `-isystem/path`.

The wrapper now consumes the following token **only** for exact
known GCC/Clang path/file options (`-isystem`, `-iquote`,
`-ivfsoverlay`, `-imacros`, `-include-pch`, etc.). Joined
`-isysroot/path` is also recognized in sysroot tracking.

A mock-compiler regression is provided at
`tools/winegcc/tests/arguments.sh`. Run it with the path to a freshly
built `winegcc`. It checks argument forwarding, not actual PE linking.
**Execution and cross-target build success are not verified yet.**

Compiler evidence:
- https://clang.llvm.org/docs/ClangCommandLineReference.html
- https://gcc.gnu.org/onlinedocs/gcc/Directory-Options.html

## Previous repairs retained — no duplicative work

Recovered relevant Water commits:
`0e57278b` (PE LTO forwarding),
`e23c628d` (prefer configured LLVM),
`b8cc30bb` (target CPU tool selection),
`8bd7d14f` (MSVC CRT default-library isolation),
`86bc87c6` (LLVM libc++ providers),
`a70f893c` and `a602ad5e` (Win16 architecture guards).

## Next PE/Win98 FE verification gates

- `winegcc` defaults bare `windows` and `console` subsystems to
  **6.0**. A Windows 98 FE PE32 executable needs a compatible
  minimum subsystem version. Verify the intended personality and
  pass an explicit version (for example
  `-Wl,--subsystem,windows:4.0`) where warranted. Do **not**
  globally downgrade newer Windows build targets.
- Default PE file alignment currently equals section alignment.
  This is not necessarily invalid, but can inflate binaries.
  Benchmark and inspect emitted PE headers before changing it.
- Keep macOS AArch64/arm64e *host* Mach-O concerns separate from
  Win98 i386 PE32, AMD64, ARM64 and ARM64EC *guest* ABIs.
- Run regression against a built `winegcc`, then link an i386
  Windows executable and inspect its imports, machine type,
  entry-point decoration, and subsystem header via `llvm-readobj`.
- The observed GitHub Actions workflows are largely CodeQL jobs;
  they do not establish Win98 FE runtime or PE link success.
