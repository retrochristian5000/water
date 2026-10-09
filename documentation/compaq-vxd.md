# COMPAQ.VXD: Windows 9x Compaq QVision mini-VDD research ledger

Status: **SOURCE-AUDITED / NO DRIVER IMPLEMENTATION** (2026-10-09).
This file records evidence and missing dependencies, not a hardware compatibility claim.

## Identity and owning component

`COMPAQ.VXD` is listed as a **Compaq video mini-driver (mini-VDD)**,
accompanying the legacy **Compaq QVision display driver `COMPAQ.DRV`**.
It is not identified as a generic support DLL for Compaq applications.

The relevant family is the **Win9x x86 virtual-display driver stack**,
with hardware-specific modes, discovery, and VDD cooperation. A native VxD
is not interchangeable with a Win32 DLL or the separate Win16 display
driver. Do not add a permissive DLL stub or assume this VxD should run on
every computer manufactured by Compaq.

Windows 95 Microsoft KB 124267 states that `DISPLAY.DRV=PNPDRVR.DRV`
is the expected loader configuration and that the actual display
driver is selected through registry configuration. Thus an installed
driver file does not establish that it was active on a particular machine.

## Distribution chronology

| Release / package | Bytes | Evidence |
| --- | ---: | --- |
| Windows 95 original DMF/non-DMF distribution (July 1995) | 17,913 | Microsoft KB 135545 / 143327 |
| Windows 95 OSR2 (August 1996) | 17,913 | Microsoft KB 158194 |
| Windows 98 First Edition (May 1998) | 17,918 | Microsoft KB 191055, `BASE6.CAB` |
| Windows Me (June 2000 inventory) | 17,417 | Microsoft KB 272253 |

The sizes are evidence of **nonidentity by size** across these
packages; they are not evidence of which control callbacks changed.
The Windows 98 Second Edition *native file* has not yet been verified
against an installation CAB. Retain this as UNKNOWN rather than
inferring it from FE or Me.

Original Windows 95 distribution places the following together:
`COMPAQ.DRV` (82,080 bytes), `COMPAQ.VXD` (17,913),
`CPQMODE.INI` (52,899), and `CPQMON.INI` (41,810).
These are strong companion-file candidates for tracing device mode
and monitor configuration. The exact role and content of the INI files
must be read before treating their names as proven formats.

## Separate Compaq graphics identities

Do not collapse these distinct entries from published legacy PnP IDs:

| ID | Display family |
| --- | --- |
| `PNP0910` | Compaq QVision |
| `PNP0915` | Compaq Advanced VGA (AVGA) |
| `PNP0918` | Matrox MGA |
| `PNP0919` | Compaq QVision 2000 |

Microsoft KB 124267 states that **QVision 2000 uses a Matrox MGA
controller**. Do not route `PNP0919` to `COMPAQ.VXD` by brand-name
similarity: inspect the actual per-device INF and hardware identity
first. A Compaq-branded computer, including an early Deskpro, need not
carry any one of these display adapters.

Microsoft KB 122710 publishes the Windows 95 QVision *driver-supported*
mode matrix:

- 640 x 480: 16, 256, 65,536 colors, and a `4G`-colors entry;
- 800 x 600: 16, 256, 65,536 colors;
- 1024 x 768: 256, 65,536 colors;
- 1280 x 1024: 256 colors.

These are **listed capabilities**, not proof that every QVision board
has the necessary VRAM, monitor timing, or supported refresh rate.
Preserve the literal `4G` notation pending verification of its pixel
format. Do not advertise modes without matching hardware context.

## Water owner and gap audit

On Water `master` at the time of this research:

- `dlls/compaq.vxd` and an exact `COMPAQ.DRV` implementation are absent.
- There is no `dlls/vdd.vxd` module in the directory inventory.
- `dlls/display.drv16` provides generic Win16 display entry points;
  many are explicit stubs. Its `ValidateMode16` tests host-exposed
  display modes; it does not implement the Compaq QVision mini-VDD.
- `dlls/vmm.vxd` exposes `VMM_VxDCall` and a subset of generic virtual
  memory services. Its existence does not demonstrate a complete
  ring-0 Windows 95/98 VxD loader or display mini-driver callback ABI.
- Existing Win9x VxD-shaped modules are **not proof** that a real
  Compaq device can be detected, initialized or driven.

The specific native VxD device ID, LE/LX object table, DDB signature,
service interface, init/control callbacks, VDD mini-driver callbacks,
register mappings, framebuffer layout, bank switching, clocking, and
matching INF/registry selection remain **UNVERIFIED**. Do not invent
ordinals or hardware ports.

## Evidence gates for implementation

1. Obtain lawful read-only copies from authentic media of the original
   1995, 1996, 1998, and 2000 `COMPAQ.VXD`, plus `COMPAQ.DRV`,
   `CPQMODE.INI`, `CPQMON.INI`, and the relevant display INF(s).
   Record source media, SHA-256, byte length and header type.
2. Decode the native VxD structure and mini-VDD registration, compare
   versions by actual byte diff, and identify supported QVision
   revisions, PnP/EISA/PCI IDs and installation registry values.
3. Trace the Windows 95/98 VDD/mini-VDD callback ABI, activation path,
   and host-vs-guest display ownership before adding a new module.
4. Separate QVision, AVGA and QVision 2000/MGA implementations, retaining
   negative match controls across all families.
5. Write nonhardware unit tests first: INF-to-device matching, mode-table
   parsing, mini-VDD callback registration, and version/profile selection.
6. Add real graphics behavior only after a reproducible guest machine,
   firmware/virtual chipset, hardware datasheet, and observable
   register/mode effects exist. A filename-only stub is not a pass.

## Evidence index

- Microsoft KB 135545 (Windows 95 DMF):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/135545
- Microsoft KB 143327 (Windows 95 non-DMF):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/143327
- Microsoft KB 158194 (Windows 95 OSR2):
  https://jeffpar.github.io/kbarchive/kb/158/Q158194/
- Microsoft KB 191055 (Windows 98 FE DMF `BASE6.CAB`):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/191055
- Microsoft KB 272253 (Windows Me):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/272253
- Microsoft KB 122710 (supported QVision modes):
  https://jeffpar.github.io/kbarchive/kb/122/Q122710/
- Microsoft KB 124267 (PNPDRVR.DRV and QVision 2000/MGA):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/124267
- QVision and related PnP IDs (independent Linux `lshw` inventory):
  https://sources.debian.org/src/lshw/02.19.git.2021.06.19.996aaad9c7-2.1/src/pnpid.txt
- QVision display / mini-driver identification:
  https://www.onecomputerguy.com/app_info-htm/95_files-htm
