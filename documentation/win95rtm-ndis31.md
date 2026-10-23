# Windows 95 RTM Ethernet / NDIS 3.1 driver ABI ledger

Status: PRIMARY 1995 DDK TEXT VERIFIED; NO VERIFIED HISTORICAL HEADER FILES
PROVIDED; NO MINIport OR FULL MAC DEVICE DRIVER IMPLEMENTED.

## Which DDK and which ABI?

Microsoft's Windows 95 DDK, About Windows 95 Network Device Drivers,
states that Windows 95 supports NDIS miniport and Full NDIS MAC drivers.
The preferred model is an NDIS miniport developed with the **Windows NT
3.51 DDK** to achieve binary compatibility with Windows 95 and NT 3.51.
NDIS 3.1 is Windows 95-specific NDIS 3.0 plus Plug and Play integration.
Win95's Full NDIS MAC model can be built from earlier NDIS 3.0 sources
using Win95 DDK tools. The DDK states NDIS 3.1 full MAC uses .VXD
rather than earlier .386 filenames and documents load/unload, pageable
code, initialization ordering, PCI/ISAPNP/PCMCIA bus and INF changes.

The actual miniport header witness must be the NT 3.51 DDK ndis.h
(and corresponding companion NT3.51 DDK declarations/build files),
not the much newer Water include/ddk/ndis.h, whose contents include
NetPnP / NDIS generations later than Windows 95. Source-identical
header contents, callbacks, layout, import names, service numbers and
compilers remain unverified until a vetted 3.51 DDK is supplied.

## Version and ownership walls

- NDIS 2: legacy DOS/Protocol.ini driver, separate from NDIS 3.1.
- NDIS 3.0: Windows for Workgroups/NT heritage; compare, do not relabel.
- NDIS 3.1 miniport: Win95 RTM preferred model, NT 3.51 DDK headers.
- NDIS 3.1 Full MAC: Win95 DDK .VXD; 3.0 MAC sources may be recompiled.
- NDIS 4/5/6: later models; never silently substitute into RTM.
- Network client/protocol stacks (TCP/IP, IPX, NetBEUI) are not NIC
  hardware miniports and do not supply PCI BAR/IRQ or Ethernet MAC logic.

## Compile-only boundary

Water include/ddk/win95rtm/network/ndis31.h is intentionally minimal.
It requires x86, and includes original ndis.h **only after**
WHP_WIN95_NDIS31_USE_HISTORICAL_DDK is explicitly set and an
independently provenance-checked NT 3.51 DDK directory is supplied FIRST.
WHP-Drivers owns the opt-in compile probe and rejects missing ndis.h
and ntddk.h from that directory; no fallback to later Water NT headers.
An NDIS status/type compile check is not a link test and does not imply
a working Win95 .VXD image, correct registration, NDIS wrapper import
table, packet send/receive path or actual Ethernet hardware.

Before implementing an NIC driver, inventory PCI/ISA PnP IDs, media type,
bus and chipset/revision, BAR/port addresses, IRQ, DMA/bus mastering,
EEPROM MAC address, and resource allocation. Record the exact real
Windows 95 RTM INF, NDIS wrapper version and driver file provenance.
Use a disposable guest with supported emulated NIC hardware; never
bind an unsupported miniport or write hardware registers in probes.

## Sources

- Windows 95 DDK About Network Device Drivers:
  https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2D14.HTM
- Windows 95 and Windows for Workgroups / NDIS 3.1 differences:
  https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2D16.HTM
- Windows 95 DDK network architecture FAQ:
  https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2D04.HTM
- Modern Microsoft ndis.h reference (contrast only, NOT RTM):
  https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/

## DMA shared-memory ownership (2026-10-23)

The portable ownership policy, DDK-gated `NdisMAllocateSharedMemory` /
`NdisMFreeSharedMemory` compilation adapter, synthetic DMA tests, and strict
cache-synchronization/PCI safety limits are tracked separately in
[`win95rtm-ndis31-dma.md`](win95rtm-ndis31-dma.md). These are **not**
Windows 95 NDIS.VXD implementations and do not establish runtime DMA support.
