# Windows XP High Definition Audio / UAA distribution ledger

Status: MICROSOFT SUPPORT SOURCES CHECKED. Original installation media,
SP3 cabinet contents, local installations and audio hardware not tested.
Reviewed: 2026-11-18.

Do not infer audio-driver availability from the broad label "Windows XP".
Preserve OS edition/build, service pack, separately applied updates,
OEM preload, exact driver files, PCI enumeration, codec identity and
working audio as independent evidence.

## Original distribution versus later support

| Target | Historical UAA HDA driver status | Boundary |
| --- | --- | --- |
| Windows 95 RTM 4.00.950 | No bundled UAA HDA support | Win16 NE and Win9x VxD interfaces, not XP's WDM bus driver |
| Windows XP RTM 5.1.2600 | No UAA HDA support in original media | Later or OEM-installed updates are separate provenance |
| Windows XP SP1 | Eligible for separate UAA updates including KB888111 | Hotfix presence and codec binding not automatic |
| Windows XP SP2 | KB888111 available separately | SP2 alone does not prove HDA bus-driver installation |
| Windows XP SP3 | UAA HDA bus support included in service-pack distribution | Still verify codec/function-driver support and actual output |
| Windows Vista | In-box UAA HDA class/bus infrastructure | Device-specific features may need manufacturer drivers |

Microsoft KB888111 describes UAA class driver 1.0a for XP SP1 and
SP2 and lists Hdaudbus.sys and Hdaudio.sys. Its registry path
Microsoft/Updates/Windows XP/SP3/KB888111 identifies servicing
metadata, not the date the feature was included in service-pack
media. An archived Microsoft community answer identifies Hdaudbus.sys
in XP SP3's service-pack cabinet; original SP3 media and exact file
versions remain a separate verification task. XP Professional x64
belongs to the Server 2003 5.2 family, not this 32-bit XP matrix.

Windows 2000 SP4 and original Server 2003 also appear as separate
KB888111 prerequisites. Do not silently group their updates, service
packs, supported device lists or binary interfaces with those of XP.

## Hardware reference and headers

Standard Intel HDA hardware registers are not Windows-specific.
Canonical hardware constants belong to include/ddk/hda/hda_regs.h
and use WHP_HDA_* names. The legacy
include/ddk/win95rtm/audio/hda_regs.h is a compatibility wrapper for
earlier WHP-Drivers code, preserving WHP_WIN95_HDA_* aliases; it
must not imply that Windows 95 shipped UAA.

The canonical header declares neither historical Win95 driver DDIs
nor WDM/KS driver DDIs. Do not map NT .SYS entry points to Win95
LE VxD or Win16 .DRV interfaces, or infer codec IDs from PCI class
04:03. Controller discovery, codec binding and audio playback are
three separate milestones.

## Provenance checklist

Capture original installation media versus installed packages; UAA
INF, Hdaudbus.sys, Hdaudio.sys and OEM codec drivers with hashes
and versions; actual device manager binding; PCI subsystem tuple;
codec identity and node graph; and tested sound playback.
A file existing on disk is not proof the corresponding driver loaded.

**Economic impact:** XP SP3's included bus driver can eliminate a
custom HDA bus-driver project for compatible hardware. XP SP1/SP2
add hotfix procurement/installation/version costs. Neither path
guarantees OEM codec features or resolves Windows 95's missing driver
ABIs. Reusing the canonical hardware header avoids duplicate
register-definition maintenance across Windows generations.

## Sources

Microsoft KB888111:
https://support.microsoft.com/en-us/topic/universal-audio-architecture-uaa-high-definition-audio-class-driver-version-1-0a-available-for-windows-xp-windows-server-2003-and-windows-2000-644d378e-d33b-a348-ccb8-2551e825042e

Microsoft UAA documentation:
https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/universal-audio-architecture

Microsoft community SP3 Hdaudbus.sys report (corroborating, not media manifest):
https://learn.microsoft.com/en-us/answers/questions/2422154/no-sound-i-need-to-installhd-audio-driver-pkg-kb88

Intel HDA architecture documentation:
https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/intel-s-hd-audio-architecture
