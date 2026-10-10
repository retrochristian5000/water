# CWBMIDI.VXD — Crystal MPU-401 MIDI VxD evidence ledger

**Status: SOURCE-AUDITED / LE INSPECTOR ADDED / NO HARDWARE DRIVER** (2026-10-09).
This file deliberately separates contemporary driver documentation from
unverified internal VxD entry points. There is no claim that Water can already
initialize Crystal ISA sound cards or drive their physical MIDI ports.

## Identity and chronology

- Microsoft's Windows 98 **First Edition** CD inventory includes
  `CWBMIDI.VXD`, **14,513 bytes**, dated **May 11, 1998, 8:01 PM**, in the
  `DRIVER21.CAB` file list (which may span adjoining CABs).
- A Windows 98 system-file inventory also lists `CWBMIDI.DRV` (9,152 bytes)
  and related `CWBAUDIO.DRV`, `CWBAUDIX.VXD`, `CWBFM.DRV`.
- Crystal Semiconductor's own Windows 95 audio driver README identifies:
  `CWBMIDI.VXD` as the **Crystal MPU-401 MIDI virtual device driver** and
  `CWBMIDI.DRV` as the **Crystal MPU-401 MIDI driver**. The README covers
  CS4232, CS4232A, CS4236, CS4236B, CS4237B, CS4238B hardware.
- An earlier Crystal driver kit (dated June 24, 1997) reports
  `CWBMIDI.VXD` at **29,326 bytes** (v4.03.2500). A DirectX 5.2 file
  inventory lists a **14,513-byte** `CWBMIDI.VXD` with a July 1997 date.
  These are separate package/version evidence points. Similar file sizes
  are **not** proof of bitwise identity or implementation parity.

The presence of these files on installation media does **not** imply that
CWBMIDI is loaded on every Windows 98 FE installation. The installation INF,
detected Crystal PnP logical devices, resources, and registry configuration
decide which driver is active.

## Keep device boundaries separate

| Component | Documented or observed role |
| --- | --- |
| `CWBMIDI.VXD` | Crystal MPU-401 MIDI virtual device, Windows 9x x86 |
| `CWBMIDI.DRV` | Crystal MIDI driver; not interchangeable with the VxD |
| `CWBAUDIX.VXD` | CS423x audio VxD; related, not the same module |
| `CWBFM.DRV` | Crystal FM synthesis driver |
| `CWBAUDIO.INF` | Install/PnP mapping, must be inspected per release |
| `MSMPU401.VXD` | Separately distributed Microsoft MPU-401 VxD; do not merge by function |

The source README identifies a distinct Crystal PnP **MPU-401 Compatible**
logical device. The exact Windows 98 PnP IDs and the port/IRQ values chosen
for each board remain **unverified** in the 1998 INF.

## Water implementation audit

- No `dlls/cwbmidi.vxd` module exists on the audited `master`.
- `dlls/krnl386.exe16/vxd.c` recognizes VxD filenames via its Win9x
  compatibility route, loads Water-provided pseudo-VxD PE modules, and has
  a small known-service dispatcher. This does **not** constitute a native
  Windows 9x LE VxD/ring-0 loader.
- `dlls/winmm` exposes application-facing Win32 MIDI functions; its API
  does not specify Crystal's private virtual-device service ABI.
- Existing `dlls/vmm.vxd` and `dlls/pppmac.vxd` are not evidence of
  functional Crystal MIDI emulation or actual MPU-401 hardware.
- `CWBMIDI.VXD` is **not a normal Win32 DLL with named exports**. Do not
  generate a fake `DeviceIoControl` or `VxDCall` success response, guess
  service IDs, force-load it on non-Crystal machines, or silently redirect
  it to `MSMPU401.VXD`.

## Read-only binary examination

Water provides `scripts/inspect-vxd.py` to validate an MZ/LE header and
extract CPU, OS, module flags, object count, and bounded resident names.
The output records SHA-256 for exact binary comparison. It does **not**
decode a VxD Device Descriptor Block (DDB), hardware ports, callbacks,
service IDs, imports, fixups, or hardware behavior.

For a lawfully obtained copy of the **Windows 98 FE** file:

```sh
python3 scripts/inspect-vxd.py /path/to/CWBMIDI.VXD --expect-size 14513
python3 -m unittest discover -s scripts/tests -p 'test_inspect_vxd.py'
```

Size matching is a *provenance warning*, not a checksum validation.
Do not confuse the separate 1997 29,326-byte vendor version with the FE
14,513-byte file. Read-only inspection never executes driver code.

## Gates before creating an actual Crystal VxD implementation

1. Verify the authentic Windows 98 FE binary, source CAB, original
   `CWBAUDIO.INF` and `CWBMIDI.DRV`; preserve hashes and raw filenames.
2. Decode the LE image, identify the DDB device ID/name, control-procedure
   address, export/import/service declarations, and VMM initialization path.
3. Verify MPU-401 register mappings, UART vs. intelligent-mode behavior,
   interrupts, Crystal CS423x resource configuration, and whether a real or
   virtual device is present.
4. Keep Win16 `.DRV`, Win9x VxD, DOS initializers, WinMM API and any
   backend synthesized MIDI devices on their respective ABI boundaries.
5. Test on a reproducible Win98 FE guest with matching emulated Crystal
   audio hardware before claiming MIDI output, VxD startup, or driver
   compatibility. Preserve negative tests for non-Crystal hardware.

## Sources

- Windows 98 FE CAB inventory, `DRIVER21.CAB`:
  https://www.localhost.me.uk/support/windows98/pages/98cabcon.html
- Microsoft KB Q191056, Windows 98 FE file inventory:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/191056
- Microsoft KB Q197017, installed Windows 98 system-file inventory:
  https://jeffpar.github.io/kbarchive/kb/197/Q197017/
- Crystal Semiconductor Windows 95 drivers README, release 2.50:
  https://driverzone.com/drivers/cirrus/sound/b95us250.htm
- Microsoft KB Q186350, DirectX 5.2 file inventory:
  https://jeffpar.github.io/kbarchive/kb/186/Q186350/
