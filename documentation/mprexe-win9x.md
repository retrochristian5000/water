# MPREXE.EXE: Windows 95/98 Multiple Provider Router process

## Status (13 October 2026)

**Research/ABI intake; no MPREXE.EXE binary is implemented in Water yet.**
This continues the June 2026 WHP MPR/MPRSERV/MPREXE investigation.
Do not claim that a generic empty PE executable is a working Win9x network
service, or automatically launch an unimplemented helper during startup.

| Component | Water owner | Confirmed status / boundary |
| --- | --- | --- |
| MPR.DLL | `dlls/mpr/` | Implements many `WNet*` entry points and provider enumeration; `WNetLogonA/W` and `WNetLogoffA/W` still return `WN_NO_NETWORK` |
| MPRSERV.DLL | Not present | Win9x-era module observed beside MPREXE; entry points and IPC contracts **not yet recovered** |
| MPREXE.EXE | Not present | Win9x 32-bit network interface service process; binary entry point, messaging and lifetime **not yet recovered** |
| MSNET32 | `dlls/msnet32/` | Separate Microsoft Network component; don't substitute it for MPREXE |
| Win9x startup | `programs/wineboot/wineboot.c` | Already processes `RunServices` and `RunServicesOnce`; not evidence that MPREXE itself is listed in either key |
| NT routing services | `programs/services/`, `dlls/mprapi/` | **Different personality**; not an MPREXE implementation |

## Evidence and limits

* A Windows 98 diagnostic module list contemporaneously identifies
  `C:\\WINDOWS\\SYSTEM\\MPREXE.EXE` v4.10.1998 as **WIN32 Network
  Interface Service Process**, and `MPRSERV.DLL` v4.10.1998 as
  **Multinet Router**, alongside `MPR.DLL` v4.10.1998.
  It corroborates three distinct Win98-era images but not their ABI or
  calling relationship. Secondary witness (period Usenet diagnostic):
  https://groups.google.com/g/uk.comp.os.win95/c/2lQoFOzSn7M
* Microsoft KB Q125714 (reviewed September 25, 1995) documents
  `RunServices`, `RunServicesOnce` and `RegisterServiceProcess` for
  **Windows 95**. The latter is obtained by `GetProcAddress` from
  `KERNEL32.DLL`; value 1 registers and value 0 unregisters a service
  process. This is **not** Windows NT SCM/ServiceMain.
  https://helparchive.huntertur.net/document/38199
* Microsoft describes the Multiple Provider Router loading provider DLLs
  in registry priority order and forwarding WNet requests. Its present
  documentation predominantly describes the NT-derived registry layout;
  it does **not** prove identical 95/98 provider registrations.
  https://learn.microsoft.com/en-us/windows/win32/secauthn/multiple-provider-router
  https://learn.microsoft.com/en-us/windows/win32/secauthn/implementing-a-network-provider
* In Water today, `dlls/mpr/wnet.c` loads NPGetCaps-bearing provider
  DLLs according to
  `HKLM\\System\\CurrentControlSet\\Control\\NetworkProvider\\Order`.
  This location is well documented for NT-derived systems. Win9x-specific
  provider configuration remains **unverified**, and must not be invented
  from NT-only docs. The MPR loader's malformed registry-string and
  path-expansion guards were strengthened in commit `ceec132`.
* Water `dlls/mpr/pwcache.c` contains an insecure legacy password-cache
  emulation in a Wine registry key, *not* evidence of the Win9x PWL
  credential format. Do not pass real secrets into that diagnostic path
  or reuse it uncritically as an MPREXE login service.

## Recovery / evidence ledger

Collect actual, lawfully available binaries or module traces from a
Windows 95 RTM/OSR2 and Windows 98 FE/SE installation separately.
Version and edition are not interchangeable.

Use a case-specific directory such as `_audit/mprserv/<case_id>/` and
capture **without** committing proprietary binaries:

* `mprexe_manifest.tsv`: OS edition, source image, path, size, SHA-256,
  PE machine, subsystem version, file/product version, image timestamp,
  and actual imports/exports
* `mprserv_file_manifest.tsv`: same for MPRSERV.DLL, including ordinals,
  forwarded exports and import dependencies
* `mpr_dll_manifest.tsv`: MPR.DLL version and exported/imported functions
* `network_provider_order.tsv`: full native registry path, value type,
  original encoded bytes, order and installation source
* `provider_dlls.tsv`: provider name/path, capabilities and observed
  initialization/teardown entry points
* `mapped_drives.tsv`: mappings and ownership; record UNC references
  in shortcuts, OLE, Access and Works records separately
* Event/order observations: who launches MPREXE; whether/when it loads
  MPRSERV; Win9x login, logoff, shutdown and restart behavior; IPC calls
  (if any); error/timeout paths

Never include plaintext passwords, PWL contents, authentication tokens or
machine-private shares in a committed audit.

## Implementation gates (must precede an MPREXE executable)

1. **Identify process contract.** Confirm historical PE32/i386 imports,
   target subsystem, entry point shape, process command-line usage,
   lifecycle and `MPRSERV.DLL` invocation with an actual Win9x artifact.
   The presence of an image or a version string is not enough.
2. **Confirm the service boundary.** Differentiate `MPREXE` launching,
   provider DLL loading, network logon callbacks, and `MPR.DLL` WNet routing
   by evidence. Don't fold these into `ROUTER.EXE`, NT SCM, or `MSNET32`.
3. **Exercise existing plumbing.** With no private credentials, compare
   Win9x provider order, failed/missing provider behavior, mapped drive
   state, login/logout and recoverable offline startup. Inspect
   `WNetLogonA/W` and `WNetLogoffA/W` before assuming success.
4. **Implement narrowly.** Once startup/IPC is attested, create
   `programs/mprexe/` and its `configure.ac` registration; use the
   existing PE32 build chain. Do not auto-start it without a selected
   Windows 9x personality and repeatable observed boot behavior.
5. **Verify.** Compile an i386 Win9x-targeted PE32 executable and check
   its import set/subsystem; compare Windows 95 and 98 traces and ensure
   it doesn't hang a login or steal provider registrations. Mark
   source-audited, build-passed and runtime-confirmed separately.

## Confirmed MPR loader-lock risk

`dlls/mpr/mpr_main.c` calls `wnetInit(hinstDLL)` from
`DllMain(DLL_PROCESS_ATTACH)`. The call traverses
`dlls/mpr/wnet.c` -> `_tryLoadProvider` -> `LoadLibraryW`, after
registry lookups. This call chain is source-confirmed. Microsoft's DLL
loader guidance prohibits `LoadLibrary[Ex]` from `DllMain` because of
loader-lock reentrancy, and cautions against registry work there:
https://learn.microsoft.com/en-us/windows/win32/Dlls/dynamic-link-library-best-practices

**Hypothesis, not a reproduced defect:** a provider load can stall or
reenter during a network-dependent login. Do not claim this caused any
particular MPREXE hang. Safely deferring provider initialization would
require auditing *all* consumers of `providerTable`, concurrency,
initialization failure, and detach cleanup. Do not merely move
`wnetInit` to the MPREXE executable or silently change MPR's API
initialization semantics.

## Outstanding contradictions / hazards

* The current Water MPR registry path is supported by modern/NT
  documentation, **not yet by primary Win9x evidence**.
* MPREXE's exact `MPRSERV` call sequence, and whether additional provider
  service DLLs are loaded in the process, remain unknown.
* `wineboot` handles `RunServices`, but this alone does not prove the
  historical MPREXE launch source or the existence of a compatible
  `RegisterServiceProcess` shim in Water.
* Preserve existing 16-bit, Win9x PE32, and NT service boundaries.
