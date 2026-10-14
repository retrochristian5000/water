# Native Windows PE module availability: chronology, personality, and ABI

## Audit of 14 October 2026

Water's build can generate native PE files for multiple CPU targets. A
successful `aarch64-windows` link **does not establish** that Microsoft
shipped the named module as an ARM64 system component. Conversely, the
presence of a 32-bit x86 DLL in a Windows-on-Arm process through emulation
does **not** make that DLL a native ARM64 binary. Keep separate:

- **Native OS component:** attested Windows package/runtime at a
  particular release and native machine type.
- **32-bit compatibility component:** x86 DLL or program used under WOW64
  in an x86 process; never mixed into the native ARM64 image.
- **Wine/Water interoperability shim:** purpose-built compatibility
  implementation that may deliberately differ from Microsoft's system
  inventory. Such a shim must be labeled as such.
- **Unverified:** absence of a located file/listing is not proof that
  Microsoft never shipped the module.

The relevant owner is `configure.ac`, whose `WINE_CONFIG_MAKEFILE`
calls use the `enable_<basename>` defaults expanded by
`aclocal.m4` into each PE architecture's `DISABLED_SUBDIRS`.
Do not remove source directories to enforce machine restrictions:
that would destroy Win9x and x86/WOW64 compatibility evidence.

## Verified exclusions from ordinary native ARM64 defaults

| Module | Native ARM64 default | Source/provenance basis |
| --- | --- | --- |
| `unicows.dll` | **Excluded**; i386 remains | Microsoft's 2001 Microsoft Layer for Unicode (MSLU) exists to adapt NT-style Unicode APIs to Windows 95/98/Me. Its import-library loader loads `unicows.dll` only on Win9x, using the OS implementation on NT. This is a 32-bit Windows 9x application-compatibility dependency, **not** an ARM64 OS Unicode layer. |
| `w95inf32.dll` | **Excluded**; i386 remains | Water's own `dlls/w95inf32/w95inf32.c` identifies a Win95/98 SETUPX shim, with Win16/Win32 thunk-data behavior and an unresolved five-argument x86 stack-decoration problem. No native ARM64 implementation or shipped OS component has been verified. Exclude the hypothetical default native port; keep Win9x/x86 coverage. |
| `ntvdm.exe` (associated executable) | **Excluded**; i386 remains | Microsoft explicitly states NTVDM supports 16-bit x86 applications only on x86 NT Windows, not on 64-bit/x64/ARM Windows. Water's DOSBox-backed NTVDM may be tested as a separately described emulator, but it must not silently masquerade as an ARM64 system NTVDM. |

Applied source defaults:

```sh
enable_unicows=${enable_unicows:-i386}
enable_w95inf32=${enable_w95inf32:-i386}
enable_ntvdm=${enable_ntvdm:-i386}
```

The normal `i386` Windows PE build still includes these modules.
A deliberate build-configuration override remains possible for future
ports with independent evidence; the default does not invent one.

## Existing protections retained (do not duplicate)

- `aclocal.m4` already maps module basenames ending in `.vxd` or
  `*16` to `enable_win16`; `configure.ac` defaults that to
  `i386`. The VxD/Win16 filter already prevents native ARM64 output.
- `wow64cpu.dll` is an **x64-only** Windows x86 emulation transition
  helper in Water. It must not be offered as an ARM64 native DLL.
- `wow64.dll` and `wow64win.dll` are genuinely relevant to
  both x64 and ARM64 WOW64 execution; do not erase them.
- `xtajit64.dll` has Water's separate **ARM64EC** policy. Its
  emulation functionality is incomplete; a stub successfully linked
  as a DLL does not mean an x64-to-ARM64 emulator is operational.
- `mf3216.dll` is a crucial **counterexample**. Despite `16` in
  its name, Microsoft's MS07-017 Windows XP x64 manifest lists an x64
  `Mf3216.dll` alongside the separate x86 WOW copy. Retain it
  outside an i386-only filter. This manifest **does not itself
  establish** whether Windows ARM64 included the module; investigate
  an ARM64 file manifest before claiming absence.

## Static source-policy regression

```sh
python3 scripts/tests/native-pe-module-personality.py
```

The regression reads the actual `configure.ac` and `aclocal.m4`,
checks that all three exclusions are i386-only by default, verifies
the per-architecture disabled-subdirectory mechanism, checks the
Win16/VxD class mapping, and preserves the x64, ARM64, and ARM64EC
WOW64 counter-controls. It is a **source-policy test**, not a
generated-configure build or Windows runtime test.

Verification needed before broader ABI changes:

1. Generate `configure` and its build graph with exactly
   `--enable-archs=aarch64`; inspect the ARM64 `DISABLED_SUBDIRS`
   and output module/install lists.
2. Repeat with `i386,aarch64` to verify 32-bit compatibility output
   stays isolated and the ARM64 lane has no invented Win9x modules.
3. Inspect authoritative Windows-on-Arm native manifests for any
   disputed module **by version and machine type**; do not infer
   installed native DLLs from APIs, filenames, or Windows 9x sources.
4. For actual built PE artifacts verify `Machine=0xAA64`, import
   architecture, exports, and runtime loadability. Keep Win9x
   personality tests on i386.

## Evidence sources

- Microsoft, *MSLU: Develop Unicode Applications for Windows 9x
  Platforms*, MSDN Magazine, October 2001:
  https://learn.microsoft.com/en-us/archive/msdn-magazine/2001/october/mslu-develop-unicode-applications-for-windows-9x-platforms-with-the-microsoft-layer-for-unicode
- Microsoft, *NTVDM and 16-bit app support*:
  https://learn.microsoft.com/en-us/windows/compatibility/ntvdm-and-16-bit-app-support
- Microsoft, *WOW64 Implementation Details*:
  https://learn.microsoft.com/en-us/windows/desktop/WinProg64/wow64-implementation-details
- Microsoft, *Running 32-bit Applications*:
  https://learn.microsoft.com/en-us/windows/win32/winprog64/running-32-bit-applications
- Microsoft security update *MS07-017*, file information for
  Windows XP x64: https://learn.microsoft.com/en-us/security-updates/securitybulletins/2007/ms07-017
- Water source evidence:
  `aclocal.m4`, `configure.ac`,
  `dlls/unicows/Makefile.in`, `dlls/w95inf32/w95inf32.c`,
  `programs/ntvdm/ntvdm.c`, `dlls/xtajit64/cpu.c`.

### Pending: do not prematurely classify

`vdmdbg.dll`, `mf3216.dll`, `w95inf32.dll` beyond the
source-specific i386 policy, and other legacy-looking module names
need independent version-specific evidence before **historical
nonexistence** is asserted. The default exclusion above is a
build-personality guard, not a universal statement that no third-party
native ARM64 port could ever exist.
