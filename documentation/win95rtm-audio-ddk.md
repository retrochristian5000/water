# Windows 95 RTM audio DDK and driver ABI ledger

Status: HISTORICAL DDK DOCUMENTATION VERIFIED; SOURCE INVENTORY CHECKED;
COMPILATION AND WIN95 GUEST RUNTIME NOT VERIFIED. NO SOUND DRIVER IMPLEMENTED.

## Source evidence: Windows 95 DDK

The 1995 DDK Audio Device Drivers guide describes waveform output and input,
MIDI output and input, auxiliary, mixer, and audio compression drivers.
Traditional audio installable driver DLLs are 16-bit Windows NE modules.
The DDK MSSB16.DEF example exports WEP, DriverProc, wodMessage, widMessage,
auxMessage and mxdMessage using EXETYPE WINDOWS, PROTMODE and segment rules.
An individual driver needs DriverProc and the entry points for its features.
Example DEF ordinals are arbitrary within that driver: do NOT pin @1..@6
for unrelated devices. The audio DDK message families are WODM, WIDM, MODM,
MIDM, AUXDM, MXDM, and the WOM/WIM/MOM/MIM callbacks.

Microsoft KB Q152539 confirms Windows 95 MCI drivers must be 16-bit;
a Win16 MCI driver can thunk to a 32-bit DLL. A companion 32-bit VxD
is a distinct hardware-side VMM/LE driver. WDM audio, WDMAud, PortCls
and NT kernel-streaming structures are NOT Windows 95 RTM audio DDIs.

## Existing WHP code (no reset)

- Water include/mmddk.h, include/mmsystem.h and include/mmreg.h:
  generic Win32 multimedia declarations; useful cross-version witnesses,
  NOT proven source-identical to the RTM 16-bit audio DDK headers.
- Water dlls/winmm/driver.c and dlls/winmm/lolvldrv.c: installable-driver
  and wave/MIDI/mixer dispatch. dlls/mmsystem.dll16: Win16 bridge.
- Water documentation/cwbmidi-vxd.md: earlier Crystal MPU-401 evidence
  distinguishes CWBMIDI.VXD and CWBMIDI.DRV and preserves Win98 differences.
- WHP-Drivers: no working audio-card driver or sound INF currently exists.

## Fail-closed Win16 include strategy

Water include/whp/win95rtm/audio/ddk_audio.h shares already validated
primitive type and far-pointer checks from display/win16_abi.h, avoiding
a duplicate ABI definition. Without a macro opt-in it imports no audio DDK
headers. With WHP_WIN95_AUDIO_USE_HISTORICAL_DDK it includes mmsystem.h
and mmddk.h from an independently verified Win95 DDK include directory.
WHP-Drivers must require both historical files and search that include
directory FIRST, ahead of OpenWatcom and Water's general Win32 includes.

Use OpenWatcom wcc (16-bit) -bt=windows -mc -bd -zu for compile-only
probes, NOT LLVM i386 COFF, wcc386, 32-bit WinMM or NT/WDM. Even a clean
compile cannot certify the NE export ABI, actual audio hardware, resource
allocation (ISA port, IRQ, DMA), PnP/INF, VxD service IDs or playback.
Do not install or activate a placeholder audio driver on a guest.

## Further verification

1. Obtain the original DDK and record media/release/header provenance.
2. Compile both Win16 probes, compare type widths, include resolution
   and calling conventions to a known Win95 RTM .DRV binary.
3. Cross-check DriverProc and message exports in its actual NE table;
   do not assume example DEF ordinals are normative.
4. Record real sound-card PnP identity, base I/O, IRQ, DMA channels,
   DSP/codec and MIDI support before implementing physical I/O.
5. Exercise only against a disposable 4.00.950 guest and known hardware.

## Sources

- Windows 95 DDK Audio Device Drivers: https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2733.HTM
- Windows 95 DDK audio message reference: https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2B24.HTM
- Windows 95 DDK MSSB16.DEF: https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2AE1.HTM
- Microsoft KB Q152539: https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/152539
- Later Windows 98 WDM distinction: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/wave-and-directsound-components
