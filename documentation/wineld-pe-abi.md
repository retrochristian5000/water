# Wineld: optional ARM64 Windows link guard

## LLVM llvm-lib versus Microsoft LIB.EXE: ARM64 comparison (14 October 2026)

**Scope:** Microsoft `LIB.EXE` is the reference for COFF static archives
and import libraries. LLVM's `llvm-lib` imitates that interface, and
`lld-link /lib` dispatches to that library-manager mode. On this host
`lld-link 17` was available but Microsoft's `LIB.EXE` executable was
**not** installed; this is an LLVM behavior test against Microsoft
documentation and the LLVM project's own compatibility analysis,
**not** a direct test run against a Microsoft executable.

Actual Linux x86_64 host probes with Clang 17 / LLD 17:
- A `/machine:arm64` static `.lib` built from a Clang-generated
  `aarch64-windows` COFF object contained machine `0xAA64`.
- Mixing ARM64 and x64 COFF objects under `/machine:arm64` was
  rejected with a machine conflict.
- `/def:period.def` with `LIBRARY period.dll` and
  `EXPORTS exported` generated an import library with the correct
  DLL name and ARM64 member objects. A real ARM64 PE32+ consumer DLL
  linked successfully and contained `period.dll` in its import data.
- `/def:bare.def` with only `EXPORTS exported` generated **bad
  output in LLVM 17**: the short-import record contained an *empty DLL
  name* and the archive's actual members appeared as `/` indexes.
  An archive file existing on disk did not mean a usable import DLL.

LLVM developers documented this same Microsoft `LIB.EXE` parity
defect on **10 July 2026** and submitted a fix to infer the DLL name
from the `.def` filename when `LIBRARY` is missing, including the
ARM64-native hybrid option. The fix must be checked in any locally
bootstrapped LLVM fork rather than assumed present merely because
the upstream patch exists:

https://lists.llvm.org/pipermail/llvm-commits/Week-of-Mon-20260706/1973989.html

Microsoft `LIB.EXE` modes and `/DEF` reference:
- https://learn.microsoft.com/en-us/cpp/build/reference/overview-of-lib
- https://learn.microsoft.com/en-us/cpp/build/reference/building-an-import-library-and-export-file
- https://learn.microsoft.com/en-us/cpp/build/exporting-from-a-dll-using-def-files

**Water fix:** `wineld`'s COFF archive audit now refuses more
than two `/` symbol-table indexes (detecting malformed unnamed import
members), refuses archives without any verified COFF members, and
validates the nonempty imported symbol and DLL name in every short
import record. Previously the audit skipped arbitrary `/` members,
which could conceal the LLVM 17 regression. For portability, a
project's DEF file should explicitly specify its intended
`LIBRARY period.dll` until the LLVM fallback is verified.

```sh
sh tools/wineld/tests/lib-exe-compat.sh tools/wineld/wineld
```

This separate regression creates genuine ARM64 and x64 objects,
runs `lld-link /lib` in static-archive and `/def:` import modes,
tests the mixed-machine rejection, links a consumer PE32+ DLL through
the valid named import library, and checks the bare-`EXPORTS`
fallback. It passes on both newer LLVM with a **correct** inferred
name and older LLVM **only if `wineld` rejects** the malformed
library with a specific diagnostic. The separate existing
`arm64-link.sh` smoke test also passed locally after the change.

**Limits:** This tests ordinary `aarch64-windows` COFF objects,
not ARM64EC/ARM64X library symbol-index hybrid rules. It does not
exercise Microsoft's actual `LIB.EXE`, Windows' loader, Water's
full `winegcc`+ `winebuild` link, ThinLTO bitcode members, or
response-file resolution. Do not claim full parity from these cases.

## Recovery and scope (14 October 2026)

Water previously shipped no `wineld` tool. The existing pipeline already
contains `winemaker` (Makefile generation), `winegcc` (compiler/link
coordination and CRT/import handling), `winebuild` (spec/export data),
and LLVM LLD (final PE/COFF linking). Upstream Wine also documents
`winegcc` and `winebuild`, not an independent `wineld` PE linker.

The new `tools/wineld/wineld` is **an opt-in diagnostic front end around
LLD's MSVC-compatible `lld-link` driver**, not a replacement for LLD,
`winebuild`, or `winegcc`. It accepts an explicit **ordinary**
`aarch64-windows` PE target and only Microsoft-linker-style arguments
after `--`. This is a narrow starting point, not a supported
cross-architecture or hybrid linker.

Example: create and validate a standalone ARM64 DLL with a documented
export, without a Wine startup/import dependency:

```sh
clang --target=aarch64-windows -c probe.c -o probe.obj
perl tools/wineld/wineld --target=aarch64-windows -- \
    /dll /noentry /out:probe.dll probe.obj
```

The test's `probe.c` defines an exported function; `/noentry` is
appropriate only for this test fixture, **not** a replacement for
`winecrt0` or real DLL startup. Use `--linker=/path/to/lld-link`
to select an installed LLVM linker, or `WINELD_LINKER` as an
environment override. `--dry-run` validates known inputs and linker
options without executing LLD.

## Verified guards

Before invoking LLD:
- Explicit ARM64 Windows target required; a macOS or GNU/MinGW
  target does not silently fall into the MSVC linker.
- `/machine:arm64` is required or added automatically; an explicit
  contradictory `/machine` is rejected.
- Every explicitly named `.o`/`.obj` COFF input is checked for
  `IMAGE_FILE_MACHINE_ARM64=0xAA64`, including the BigObj format.
- Explicit `.lib`/`.a` archive members are checked for COFF
  machine, including short-import objects; archive symbol/name
  indexes are excluded. Truncated archives and thin archives are
  rejected instead of being silently misread.
- A `.dll` output must have `/dll`, not just a DLL-like filename.
- An input archive conflicting with the DLL's implicit or explicit
  import-library output path is rejected to protect existing inputs.

After LLD successfully links, the tool verifies the `MZ` and `PE`
signatures, final `Machine=0xAA64`, PE32+ optional-header magic
`0x20B`, and the DLL image-characteristic flag when `/dll` was
requested. LLD's own error code is propagated; tool checks never
silence a failed link or rewrite the ABI.

**Important ARM64EC boundary:** Microsoft documents
`IMAGE_FILE_MACHINE_ARM64EC=0xA641` for *object files only*. A
hybrid ARM64EC/ARM64X final image can instead advertise x64 or ARM64
and requires further hybrid metadata validation. Consequently,
`wineld` **rejects** ARM64EC today; it does not equate ARM64EC to
ordinary ARM64 or pretend final-image machine 0xA641 is valid.

## Observed tests

The opt-in smoke test was executed in a Linux x86_64 tool container
with Clang 17, LLD 17 and llvm-ar:

```sh
sh tools/wineld/tests/arm64-link.sh tools/wineld/wineld
```

It generated a real ARM64 PE32+ DLL, checked a matching archive,
rejected x64 COFF input and a mixed ARM64/x64 archive, checked
conflicting `/machine`, unsupported ARM64EC and MinGW targets, and
required `/dll` for a DLL output. It also reproduced and then guarded
against an implicit import library clobbering a same-basename
`.lib` used as a link input. Perl syntax check and this smoke test
passed; the GitHub source/test blobs were compared byte-for-byte with
the locally tested files before publication.

**Not yet verified:** a complete Water `winegcc`/`winebuild` DLL
build or native Windows loader execution. This tool is **not installed
or called by the default build pipeline**. The user must invoke it
explicitly. Direct linker options are those of `lld-link`; the
script does not translate `winegcc` or GNU `ld` arguments.

## Limits and next validation gates

Input scanning covers **explicitly named files only**. It does not
yet expand `@response` files, resolve `/defaultlib` or
`/libpath` searches, prove import-library symbol compatibility,
analyze `.res`/PDB contents, or inspect all nested/LTO members.
Bitcode-only and thin archives require an additional method.
Paths are checked for straightforward input/output collisions but
this is not a sandbox or a security boundary against malicious paths.
Larger final PE structure, relocations, load configuration, actual
startup behavior and Windows loader ABI remain separate tests.

The correct next integration gate is to capture the actual
`winegcc`-generated `lld-link` argv and `winebuild` `.spec.o`
objects, then validate them in isolation with `wineld`. Do not
automatically replace a compiler driver with this *linker* front
end or route GNU/MinGW `ld.lld` arguments into it.

Sources:
- Microsoft PE/COFF format:
  https://learn.microsoft.com/en-us/windows/win32/debug/pe-format
- Microsoft ARM64EC object and final-image distinction:
  https://learn.microsoft.com/en-us/windows/arm/arm64ec
- LLVM LLD's MSVC-compatible linker driver:
  https://lld.llvm.org/windows_support.html
