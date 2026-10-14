## ARM64 Windows Clang command audit (October 13, 2026)

**Resolved at source level:** `tools/winegcc/winegcc.c` previously added
Clang-only `-target` and `-fuse-ld=lld` to *any* explicit
`--cc-cmd` on a PE target, even if the user selected a GCC-compatible
compiler. That could stop `aarch64-windows` compilation before it
generated DLL objects. The helper now recognizes Clang commands (including
common `ccache`/`sccache`/`distcc`/`env` wrappers) and does not
force Clang options on explicit GCC commands. The
`tools/winegcc/tests/arguments.sh` regression checks ARM64 Windows
Clang, an explicit cross-GCC override, and a cached Clang invocation.
This is not a claim that all cross-GCC drivers accept MSVC ABI link flags.

**Actual compiler/linker probe (separate from Water):** Clang 17 on a
Linux x86_64 host accepted `-target aarch64-windows` with LLD and
emitted a PE32+ `IMAGE_FILE_MACHINE_ARM64` (`0xaa64`) DLL from a
standalone C export. Its PE header had `FileAlignment=0x200` and,
when explicitly supplied Water's `/ALIGN:0x10000`, had
`SectionAlignment=0x10000`. LLD emitted its documented warning
`/align specified without /driver; image may not run` but returned
success. An otherwise equivalent default LLD link emitted
`SectionAlignment=0x1000` and no alignment warning. The minimal probe
did **not** invoke Water's `winegcc` executable, `winebuild`, import
archives, or Windows loader; no complete Water DLL build is verified.

Do **not** use `/driver` just to silence the warning on a user-mode
DLL. Keep the existing explicit ARM64 64 KB alignment policy while
investigating Wine compatibility with 16 KB host pages. Windows ARM64
itself normally uses 4 KB pages, and Microsoft's conventional
`/ALIGN` linker default is 4 KB; the 64 KB setting is a separate
compatibility policy, not a Windows ARM64 ABI mandate. Distinguish
`aarch64-windows` (MSVC ABI) from `aarch64-w64-mingw32` (GNU ABI).

Evidence:
- https://learn.microsoft.com/en-us/cpp/build/reference/align-section-alignment
- https://devblogs.microsoft.com/oldnewthing/20210510-00/?p=105200
- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

**Remaining gates:** Run the ARM64-target `winegcc` argument regression
with an actual built Water wrapper; inspect the compiler command,
`winebuild`-generated `.spec.o`, startup library selection, LLD
command, COFF import archives and final PE exports with LLVM tools.
Do not conflate the host aarch64/arm64e Mach-O ABI with the guest
aarch64-windows or ARM64EC COFF ABI.

## -isysroot scope audit (October 13, 2026)

The `isysroot` variable is **active**, not dead: `-isysroot PATH` and
`-isysrootPATH` populate it, and the non-Apple compiler include search
prefers it over Water's explicit `--sysroot`. The argument itself continues
to pass to Clang/GCC. On macOS, the Apple SDK is used by the compiler but
must **not** redirect Wine's installed include paths.

A September 23 Darwin-prefix fix (`ac228053`) accidentally moved
`incl_dirs` inside `#ifdef __APPLE__` without a matching non-Apple
declaration. Commit `a1cbcf9` restores the previous non-Apple search
list (`INCLUDEDIR`, `/usr/include`, `/usr/local/include`) while keeping
macOS restricted to `INCLUDEDIR`. The argument forwarding test now checks
non-Apple include rooting and `-isysroot` precedence over `--sysroot`.
This is source-audited; an actual Water host build and mock-compiler
regression run remain unverified.

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
- The normal PE file alignment default is now 512 bytes, independent
  of virtual SectionAlignment. Verify the actual emitted PE headers,
  measure binary sizes, and test Windows 98 FE loading before attributing
  any remaining problem to file alignment.
- Keep macOS AArch64/arm64e *host* Mach-O concerns separate from
  Win98 i386 PE32, AMD64, ARM64 and ARM64EC *guest* ABIs.
- Run regression against a built `winegcc`, then link an i386
  Windows executable and inspect its imports, machine type,
  entry-point decoration, and subsystem header via `llvm-readobj`.
- The observed GitHub Actions workflows are largely CodeQL jobs;
  they do not establish Win98 FE runtime or PE link success.

## PE alignment policy (2026-10-09)

The original wrapper set default `FileAlignment` equal to
`SectionAlignment`. For i386/x64 PE output that produced a
4096-byte on-disk file alignment; for ARM64/ARM64EC it used 65536.
This is unnecessarily padded compared to the documented 512-byte
PE default, although those older values are not inherently invalid.

The wrapper now defaults normal PE images to **FileAlignment=0x200**
(512 bytes), retaining the existing virtual **SectionAlignment=0x1000**
for i386/x64 and **0x10000** for Water ARM64/ARM64EC. Explicit
`-Wl,--file-alignment` overrides still win. Sub-page PE section
alignment (<0x1000) still defaults file alignment to the same value,
preserving the special matching file/RVA layout rule. Non-PE formats
are unaffected. Do not assume arbitrary sub-512 layouts are supported
by Windows 98 without separate tests.

Both the MSVC-compatible COFF `-filealign:/-align:` and MinGW driver
paths consume the corrected values. The mock-link regression at
`tools/winegcc/tests/pe-alignment.sh` checks emitted linker options
for each supported target and override, but does not validate a PE
file or execute on Windows 98 FE.

Microsoft PE specification:
https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

**Testing gates:** build the host `winegcc` and run the test script,
then generate real i386 PE32 samples with LLD and inspect
`FileAlignment`, `SectionAlignment`, `SizeOfHeaders`, file offsets,
machine type and subsystem version using `llvm-readobj`. Measure actual
on-disk byte size. Earlier ARM64 LLD alignment guards remain intact;
the separate subsystem-version-6.0 issue is not fixed here.
