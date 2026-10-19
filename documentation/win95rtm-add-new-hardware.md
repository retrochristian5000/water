# Windows 95 RTM Add New Hardware Wizard: driver-installation evidence ledger

Status: HISTORICAL DOCUMENTATION CROSS-CHECKED / WATER SOURCE AUDITED /
RTM GUEST WIZARD AND ORIGINAL BINARIES NOT YET INSPECTED. NO NEW WIZARD
OR DRIVER INSTALLATION CODE; NO DEVICE DETECTION OR REGISTRY MODIFICATION.

## Historical workflow and chronology

Microsoft's Windows 95 documentation distinguishes two paths:

1. Plug and Play device appears during boot or insertion: bus/BIOS
   enumeration identifies the device, the Configuration Manager tries to
   allocate resources, and Setup looks up a matching driver/INF. A missing
   driver causes a manufacturer-media prompt. This does not require the
   user to initiate the Add New Hardware applet.
2. Legacy non-PnP device: Control Panel -> Add New Hardware -> Next.
   The user can request automatic detection or choose the manual
   category -> manufacturer -> model -> Have Disk route. The January 25,
   1996 Microsoft Q142984 instructions explicitly describe choosing No
   to automatic search and selecting a manufacturer's OEMSETUP.INF.

Do not merge the manual Add New Hardware Wizard with the automatic
New Hardware Found/update-driver flow, Device Manager properties, or
separate Modems, Network, Printers, and PCMCIA installation interfaces.
The late-1990s and 2000+ wizards have different UI and installer APIs.

The 1995 DDK Plug and Play introduction explains the device-node model:
bus enumerators discover devices; resource arbitrators allocate IRQ,
I/O port, DMA and memory requirements. Legacy devices have registered
static requirements, generally considered before configurable PnP
resources. An INF entry alone does not prove hardware is present or
that a suitable driver has loaded.

## Evidence matrix: Microsoft versus Water implementation

| Contract | Windows 95 evidence | Audited Water implementation | Status |
| --- | --- | --- | --- |
| Control Panel wizard | Add New Hardware launches guided legacy detection/manual selection | dlls/shell32/cpanelfolder.c enumerates generic Control Panel content; a genuine RTM wizard host/executable was not identified in this audit | RTM Control Panel applet unverified |
| Hardware enumeration/resources | PnP BIOS, bus enumerators, Configuration Manager, arbitrators, static legacy resources | dlls/cfgmgr32 provides a modern compatibility implementation; not evidence of Windows 95's CONFIGMG/VMM ABI | historical interfaces need separate guest evidence |
| Driver INF selection | [Manufacturer], model, device class, Have Disk/OEMSETUP.INF, vendor sources | dlls/setupx.dll16/infparse.c IpOpen16 delegates to SetupOpenInfFileA with INF_STYLE_WIN4 | a parsing bridge, not a wizard |
| SetupX device list | class/compatible driver listings selected by Setup | dlls/setupx.dll16/devinst16.c DiBuildClassDrvList16, DiBuildCompatDrvList16 and DiCallClassInstaller16 are stubs | install path incomplete |
| Install selected device | selected driver files, class installer, registry/resources and potential restart | dlls/setupx.dll16/setupx.dll16.spec marks DiSelectDevice (312), DiInstallDevice (313), DiAskForOEMDisk (315), and DiInstallDriverFiles (401) as stubs | no real Win95 installation |
| Newdev API | DO NOT infer Windows 95's wizard implementation merely from its name | dlls/newdev/main.c InstallNewDevice is a stub; newer UpdateDriverForPlugAndPlayDevices implementation is not a verified RTM entry point | version-mismatch risk |
| Generic SetupAPI | INF and device setup support; exact 16/32-bit split must be observed in an RTM guest | dlls/setupapi includes a large modern API surface and INF parser | modern implementation is not Win95 RTM proof |

Only the historically documented UI and 1995 source descriptions
are authoritative *about Windows 95*. The Water source paths above
describe the CURRENT FORK, not the original Microsoft file layout.
The 16-bit SetupX ordinal namespace must not be copied into SetupAPI
Win32 exports, a Windows VxD service table, or a later NEWDEV DLL.

## INF and driver-package integrity

An original Windows 95 hardware list is strongly dependent on INF files
and driver index databases; Microsoft Q137377 documents OEM<n>.INF
entries associated with earlier Windows 3.1 drivers confusing the
manufacturer/model lists. Windows 95 Q129260 describes DRVIDX.BIN and
DRVDATA.BIN driver indexes and use of SETUPX.DLL during Setup.

Q182723 (1998) documents an important **release split** when default
INF files are missing/damaged:
- Retail Windows 95: `Windows was unable to add this <device>.`
- Windows 95 OSR2: an unknown device followed by a manufacturer disk
  prompt. The article applies to both but does NOT equate their paths.

The existing WHP-Drivers packaging/win95rtm/R6410D.INF is deliberately
NOT INSTALLABLE: its [Manufacturer] model list is empty, and no actual
driver install section, CopyFiles, AddReg or PCI hardware binding exists.
The exact fail-closed preflight tests must stay in place. It is NOT a
valid Have Disk package, and adding the device to a list is NOT evidence
that hardware is detected, resources assigned or a display driver works.
Do not copy WHP INF research scaffolds into WINDOWS\\INF or run
untrusted vendor installation code on a real host.

## Sound-card and Ethernet drivers: implications

- Sound: the wizard may expose a sound-card model and resource choices;
  genuine RTM audio uses Win16 multimedia .DRV boundaries, optionally
  companion 32-bit VxDs. Save the class, hardware ID where present,
  vendor INF, IRQ/DMA/I/O range, and distinct audio/MIDI devices.
- Ethernet: capture ISA PnP/PCI/PCMCIA ID, class, IRQ, I/O or MMIO
  resources, NDIS 3.1 driver package and binding data. A detected NIC
  and working TCP/IP network stack are different success conditions.
- Display: retain fallback VGA.DRV; a wizard-selected model is not
  evidence of verified DIBENG, mini-VDD or actual GPU initialization.
These three paths should share an inventory/INF selection methodology
but MUST NOT be flattened into one driver ABI.

## Safe next verification in a disposable Win95 4.00.950 guest

1. Before changes: capture exact SYSTEM, Control Panel, SETUPX.DLL,
   SETUPAPI.DLL and CONFIGMG-related file versions, plus a VM snapshot.
   Do not infer the original wizard module filename from Water's paths.
2. Open Add New Hardware without triggering live invasive detection.
   Record all screens, automatic/manual choices, device classes and
   manufacturer/model list as OBSERVED (not guessed) RTM data.
3. Use authentic, lawfully obtained installation media in an isolated
   VM, capture the original INF and target driver filenames/hashes;
   inspect but do not install unsigned or unmatched device drivers.
4. Compare pre/post device enumeration, Device Manager resource
   assignments, VxD/DRV loads and log/registry changes in snapshots.
   Keep explicit negative cases: missing INF, bad class, unknown ID,
   unsupported resources, selected but unbound device.
5. Compare a separate OSR2 snapshot for the Q182723 error-path split.
   Never project OSR2 behavior onto 4.00.950 without confirmation.
6. Only after reproducing the actual SetupX contracts should Water
   implement missing installer functions; no fabricated success,
   unconditional registry write, raw IRQ/port probe or host disk access.

## Sources

- Microsoft Windows 95 end-user guide, Installing New Devices:
  https://techshelps.github.io/MSDN/DNWIN95/HTML/S64D5.HTM
- Microsoft Q142984 (reviewed January 25, 1996), How to Install New
  Hardware: https://helparchive.huntertur.net/document/43903
- Windows 95 DDK Plug and Play Introduction:
  https://techshelps.github.io/MSDN/WIN95DDK/HTML/S247B.HTM
- Lee Fisher, Win32 Application Support for Plug and Play (Jan 18, 1995):
  https://techshelps.github.io/MSDN/BACKGRND/html/pnpapp.htm
- Microsoft Q129260, Windows 95 Setup and detection:
  https://techshelps.github.io/MSDN/KB/Source/win95x/q129260.htm
- Microsoft Q137377, OEM INF entries in hardware lists:
  https://techshelps.github.io/MSDN/KB/Source/win95x/q137377.htm
- Microsoft Q182723, RETAIL versus OSR2 bad-INF symptoms:
  https://techshelps.github.io/MSDN/KB/Source/win95x/q182723.htm
