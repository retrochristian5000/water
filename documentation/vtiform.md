# VTIFORM.EXE historical and implementation ledger

Status: **SOURCE-IMPLEMENTED APPROXIMATION; NOT BUILD/RUNTIME VERIFIED**.
Audit date: October 9, 2026. Target: Win9x Win32 executable, not a
kernel, VxD, DLL or base-system utility.

## Chronology and evidence

- **FrontPage 1.1:** Microsoft KB 153045 lists `vtiform.exe`
  (191,488 bytes; version 1.1.2.6) as a page-template wizard file
  associated with `vtiform.wiz`. It predates Windows 98.
- **Internet Explorer 4:** Microsoft KB 181589 lists `VTIFORM.EXE`
  (237,056 bytes; dated August 7, 1997) and `VTIFORM.INF` (121
  bytes) in `FPESETUP.CAB`, the FrontPage Express installation
  cabinet. It also lists `FPXPRESS.EXE`, `FPXPRESS.INF`,
  `VTIHOME.EXE`, `VTIHOME.INF`, `SETDEFED.EXE`,
  `FP20HTP.DLL`, `FP20IME.DLL`, `FP20TL.DLL`,
  `FP20UTL.DLL`, `LEAD52N.DLL`, and HTML/INF templates.
- **Internet Explorer 5:** Microsoft KB 221526 records
  `VTIFORM.EXE` at the same 237,056-byte size in `FPESETUP.CAB`,
  with package changes. Equal byte size does not prove equal bytes.
- Later FrontPage/Office products reuse `VTIFORM.EXE` as a wizard
  name. Do not flatten Office 2000/XP/2003 with IE4-era versions.

**Caution:** Seeing this in Windows 98 FE *media* does not prove
the wizard was in a fresh mandatory OS installation. The confirmed
package is the separate IE/FrontPage Express feature cabinet.

Water previously had no `programs/vtiform`, `programs/fpxpress`
or `VTIFORM.EXE` implementation.

## Added to Water

`programs/vtiform` independently implements a standalone Win32
HTML form-template creator. It accepts a title and up to 20 named
fields, uses the standard Save As dialog and writes escaped HTML.
Generated form *submission is disabled*, since no response handler
or FrontPage server extensions are configured. The clone does not
send mail or store submitted information.

`vtiform.exe --self-test` checks parsing and HTML escaping
without network usage; **test has not been run**.

This is not the original FrontPage wizard user interface, and it does
not implement FrontPage host activation, `.wiz` templates, form
processing, web publishing, COM/IPC, or installation registration.
It is a functional standalone approximation only.

## Required follow-ups

1. Hash original FE media's `FPESETUP.CAB` and extract authentic
   `VTIFORM.EXE`, its INF, and the original templates.
2. Identify PE version/exports/imports, command-line/host invocation,
   any external dependencies and the FrontPage integration contract.
3. Distinguish FrontPage 1.1, IE4/IE5, and later Office generations.
4. Compile the Water program, run `--self-test`, and verify the UI
   and Save As output on Win98 FE and another Win32 baseline.
5. Only implement stronger compatibility after native evidence and
   regression tests. Do not infer success from filename matching.

## Sources

- https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/153045
- https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/181589
- https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/221526
