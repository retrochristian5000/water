# Modern LLVM libc++ header wiring — migration audit

**Audit date:** 14 October 2026. **Source:** Water `master`, building the
pinned LLVM libc++ Microsoft-ABI PE provider. This ledger supplements, rather
than resets, `libcxx-powerpc-migration.md`, the i386 C++ ABI tests, and the
ARM64 RTTI/COM audits.

## Original routing and discovered contamination risks

The project has **two different C++ implementations** during migration:

- LLVM libc++ 24: staged in
  `build/llvm-libcxx-pe/<arch>/include/c++/v1`, with static
  `libwhp-libcxx.a`, `vcruntime`, COFF member audits and
  `_LIBCPP_ABI_MICROSOFT` / `_LIBCPP_ABI_VCRUNTIME` checks.
  Prepared for x64/ARM64/ARM64EC and optionally i386.
- Water's older libc++ 8: **still necessary** for unmigrated
  PowerPC Windows, ARM32, and i386-by-default paths.
  Its headers are intermingled with MSVCRT headers under
  `include/msvcrt`, including extensionless C++ files
  `__config`, `string`, `vector`, `type_traits`, etc.

Previously, `build.sh` correctly staged **only `*.h`** into a filtered
`msvcrt-headers` overlay for the libc++ CMake compile. However, its
*postbuild probe* compiled using the unfiltered
`-isystem "$SOURCE_DIR/include/msvcrt"`. That could hide errors due to
mixing libc++ 8 and 24 headers, and the validator no longer reproduced the
actual build search path.

Separately, `scripts/install-libcxx.sh` installed modern C++ headers
and its library but no filtered CRT directory. Installed `winegcc` added
`wine/msvcrt` to all MSVCRT C++ builds. If that legacy directory was
retained for PowerPC/ARM32/i386, a missing C++ standard header could
fall through to the old libc++ instead of failing clearly.

## Corrections

1. `build.sh` now uses `-idirafter "$whp_libcxx_crt_headers"` for its
   postbuild C++ ABI validation, matching the filtered CMake build input.
   The generated `<__config>` comes from LLVM libc++ 24, not the old CRT
   source directory.
2. Cached CRT header overlays are refused and reconstructed if
   `corecrt.h` or `vcruntime_exception.h` is missing, or if an
   unexpected non-`*.h` file such as `__config` or `string` has
   entered the filtered directory. Symlinks are rejected as well, so
   a legacy C++ file cannot be exposed under a harmless-looking name.
   Paths remain portable to macOS/BSD
   `find`; the cache still keys on all source `*.h` checksums.
3. The libc++ provider cache signature also includes checksums of
   `windef.h`, `guiddef.h`, `winnt.h`, `windows.h`,
   `winbase.h`, and `objbase.h`. These live SDK headers are transitively
   used by the WIDL bootstrap projections; previously, the cache
   fingerprint only covered WIDL IDL inputs and filtered CRT inputs,
   not these ABI header revisions.
4. `scripts/install-libcxx.sh` now installs the same filtered
   `*.h` CRT tree for **each modern PE architecture** at
   `<includedir>/wine/c++/<arch>/crt`. The existing older
   `wine/msvcrt` headers are retained if any selected architecture still
   needs the legacy provider. An installer refusing a contaminated CRT
   source never silently copies extensionless libc++ 8 C++ headers.
5. For **installed modern PE C++** consumers, `winegcc` detects the
   installed modern libc++ headers and switches the automatic MSVCRT
   include to the same per-architecture filtered `crt` directory,
   dropping *all* its automatic legacy `wine/msvcrt` fallback paths
   for that compiler invocation. If modern headers exist but the matching
   filtered CRT is missing, it reports an incomplete development install
   instead of silently mixing the two providers. Native C, host builds,
   and older C++ paths without a modern provider retain their existing
   MSVCRT header routing.

**Scope caveat:** this prevents *automatic* fallthrough through the
Water-owned include path. A caller can still explicitly inject a
nonstandard `-I` pointing at legacy headers; tracing actual compiler
include lists remains a separate validation step. The staged CRT
directory is not an alternate complete SDK and never replaces WIDL's
real generated `unknwn.h`/`oaidl.h` COM contracts.

## Checks

The two no-network regressions:

```sh
sh scripts/tests/libcxx-header-isolation.sh
sh scripts/tests/libcxx-sdk-cache.sh
```

extracts Water's actual `prepare_llvm_msvcrt_headers()` shell function,
stages synthetic modern CRT/legacy C++ inputs, poisons a supposedly
valid cache with `__config` and a symlinked old C++ header, then confirms
the helper rebuilds it.
It mutates a CRT `*.h` source to check invalidation; executes the
real installer into a temporary root; checks the architecture-specific
CRT tree excludes old C++ names; verifies the old headers remain
available under mixed-provider mode and are removed under modern-only
mode; and checks the build/winegcc source-level routing contracts.

The separate SDK-cache test executes the real
`prepare_llvm_windows_sdk_headers()` function on an isolated miniature
Windows/COM header graph. It checks no-op cache reuse, invalidation
after changing `winnt.h`, `objbase.h`, and `unknwn.idl`, restoration
of a deleted generated `unknwn.h`, and rejection of a missing
`guiddef.h`. This guards against recent ARM64/COM header fixes
being hidden by a stale libc++ provider cache; it does not assert
PowerPC Windows C++ ABI readiness.

**Verification levels:** the GitHub sources, scripts and installed CRT
routing were reviewed and committed. The regression is provided for a
local Water checkout but has **not yet been executed** through that
checkout, and a full LLVM libc++ build and Windows runtime test have
not been run for this change. Source checks cannot replace compilation
or C++ exception/RPC/runtime testing.

## PowerPC and retirement gates

`powerpcle-windows` currently emits ELF on the locally probed LLVM 17,
not Windows PE/COFF. The newer libc++ cannot be promoted to that lane
merely by adding include paths. Retain legacy PowerPC headers/runtime
until the exact PowerPC Windows machine, COFF linker relocations,
exception/RTTI/CRT ABI, and actual libc++ archive pass independent
testing. `WATER_LIBCXX_LEGACY_POLICY=error` remains the way to reject
old libc++ fallback when testing a deliberately narrowed modern-only
architecture set.

Do not delete `libs/c++`, `libs/c++abi` or legacy extensionless CRT
headers while PowerPC/ARM32/i386 consumers still need them. The next
productive validation is to run the new regression, then a complete
Clang+CMake libc++ bootstrap and `winegcc` C++ consumer smoke test for
each supported PE architecture, with the compiler's include trace
checked for libc++ 8 paths.
