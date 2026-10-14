## Windows ARM64 linker flag repair (October 13, 2026)

Water's `PLATFORM_WINDOWS` link command emitted
`-Wl,-filealign:0x200,-align:0x10000` for ordinary AArch64 DLLs.
Independent Clang 17 + LLD 17 probes on Linux x86_64 confirmed that
any explicit `/ALIGN`, even `/ALIGN:0x1000`, produces:
`lld-link: warning: /align specified without /driver; image may not run`.
The PE32+ ARM64 output was otherwise generated successfully.
**This warning alone does not prove loader failure or ABI corruption.**

The source-level repair now uses the standard `0x1000` PE section
alignment for an ordinary `aarch64-*-windows` MSVC-ABI image and omits
`/ALIGN` entirely when the selected alignment is `0x1000`.
The separate `FileAlignment=0x200` stays explicit. A repeat direct
Clang/LLD smoke test without `/ALIGN` produced PE32+ ARM64 machine
`0xAA64`, section alignment `0x1000`, file alignment `0x200`
and an exported C function, without the `/ALIGN` warning.

**Deliberately unchanged:** ARM64EC, explicitly requested ARM64X
(`-marm64x`), and the GNU/MinGW ARM64 path retain their separate
previous `0x10000` default section-alignment policy. A user override
such as `-Wl,--section-alignment,0x10000` continues to request
`/ALIGN:0x10000` for ordinary ARM64 Windows images. Do not add
`/DRIVER` to a user-mode DLL to suppress the warning.

The mock-link regressions in
`tools/winegcc/tests/pe-alignment.sh` distinguish ordinary ARM64
from ARM64EC/ARM64X and explicit alignment overrides. These
assertions and source changes are published; the *Water binary*
itself has not been rebuilt in this session and Windows ARM64 loading
has not been tested. File alignment, section alignment, image machine,
DLL imports, entry point and export table must still be checked
together before any ABI-compliance claim.

References:
- https://learn.microsoft.com/en-us/cpp/build/reference/align-section-alignment
- https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- https://lld.llvm.org/windows_support.html
- https://sources.debian.org/src/llvm-toolchain-22/1%3A22.1.8-1/lld/test/COFF/align.s

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
DLL. Use the normal 4 KB section alignment for ordinary Windows ARM64
images; the prior 64 KB default is retained only for separate
ARM64EC/ARM64X and GNU paths pending their own loader checks. Do not
conflate PE section alignment with host page size. Distinguish
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
(512 bytes), with virtual **SectionAlignment=0x1000** for ordinary
Windows i386/x64/ARM64 and **0x10000** for the still-separate
ARM64EC/ARM64X or GNU ARM64 policies. Ordinary MSVC-style PE links
omit a redundant `/ALIGN` when using the 4 KB default. Explicit
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
