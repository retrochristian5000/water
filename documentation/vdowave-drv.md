# VDOWAVE.DRV — Windows 98 FE VDOWave video codec

Status: **SOURCE-AUDITED / VFW FOURCC DISPATCH TESTED IN MOCK /
PE DRIVER INSPECTION UTILITY ADDED / NO VDOWAVE DECODER** (2026-10-09).

## Verified historical identity

- The Windows 98 FE CD inventory (Microsoft KB Q188438) lists
  **VDOWAVE.DRV** in **WIN98_59.CAB**: **82,432 bytes**, timestamp
  **1998-05-11 20:01**. This identifies a distribution artifact, not its
  actual hash, PE header, export table, or internal codec version.
- VDONet's **VDOWave** is a wavelet-based **video** codec, not a wave
  audio (waveOut) driver or virtual device. It is associated with the
  Video for Windows (VfW) installable-codec/ICM driver architecture.
- Contemporary documentation distinguishes **VDOWave 2.0** with FOURCC
  **VDOM** and **VDOWave 3.0** with FOURCC **VDOW**.
  Neither FOURCC is inferred merely from the filename. Do not merge
  them or assume that the Windows 98 FE 82,432-byte driver handles both.
- Windows 98 SE OEM SYSTEM.INI records, including Toshiba machines,
  contain the mapping [drivers32] VIDC.VDOM=vdowave.drv.
  This is a **Win32** VfW codec registration, not Win16 SYSTEM.DRV
  or the Win9x VxD driver interface.
- NetShow 2.0 distributed decode-only and separately available encoder
  components. Decoder presence does not imply encoder functionality.

## Water's existing implementation boundary

Water already implements generic VfW codec discovery and loading:

| Owner | Relevant code | Contract |
| --- | --- | --- |
| MSVFW32 | dlls/msvfw32/msvideo_main.c | ICInfo, ICOpen, ICLocate, ICM dispatch |
| WinMM | dlls/winmm/driver.c | [drivers32] resolution, LoadLibrary, DriverProc |
| Existing video codecs | dlls/iccvid, dlls/msvidc32, dlls/msrle32 | Other, unrelated compressed formats |

MSVFW32 enumerates registered drivers and SYSTEM.INI [drivers32] mappings.
WinMM can load an installable 32-bit PE driver if its DriverProc exists.
**No VDOWave stream decoder is implemented or included in Water**.
Do not register an empty VDOWAVE.DRV clone by default: it could claim
support for a format that cannot be decoded. Do not substitute
Cinepak, Microsoft Video 1, RLE, H.263, or an unrelated wavelet format
based on superficial similarity.

## Current changes

- Added a test in dlls/msvfw32/tests/msvfw.c that installs two
  temporary *in-process* mock handlers and verifies that ICM routes
  VDOM and VDOW as independent FOURCCs. The mock explicitly rejects
  decompression and is removed at the end of the test; it does not
  create a new codec module, edit SYSTEM.INI, or claim playback support.
- Added scripts/inspect-vfw-driver.py, a **read-only MZ/PE inspector**.
  It checks PE format, machine, DLL characteristic, export names,
  ordinals, forwarded exports and SHA-256. It rejects a Win16 NE
  image, truncated section data and unreasonable export counts.
  It does not execute the driver or guess its stream bitstream grammar.
- Added synthetic PE/invalid-image regression tests in
  scripts/tests/test_inspect_vfw_driver.py.

To check a lawfully obtained **Windows 98 FE** copy:

    python3 scripts/inspect-vfw-driver.py /path/to/VDOWAVE.DRV --expect-size 82432 --require-driverproc
    python3 -m unittest discover -s scripts/tests -p test_inspect_vfw_driver.py

A matching file size does **not** establish bitwise identity. The
actual Windows 98 FE file has not been supplied or inspected here.

## Next compatibility gates

1. Extract the original WIN98_59.CAB member and preserve provenance,
   exact hash, PE characteristics and exports. Compare the 82,432-byte
   file against other VDOWave releases without assuming equivalence.
2. Verify exact Win98 FE [drivers32] registrations, file destinations,
   installed version metadata, and presence or absence of VDOM/VDOW.
3. Use an isolated Windows 98 FE guest with an authentic sample AVI to
   record ICInfo, ICOpen, ICM_GETINFO, ICM_DECOMPRESS_QUERY, format
   negotiation and output frames, with a separate negative/unsupported
   format sample.
4. If full clean-room decoding is needed, derive the actual bitstream
   syntax from legitimate samples and independent documentation;
   retain separate version paths and frame safety checks.
5. Only then add a correctly scoped implementation of the PE/VfW
   DriverProc ABI and decoder to Water. Avoid unsupported success.

## Sources

- Microsoft KB Q188438, Windows 98 CD listing, WIN98_59.CAB:
  https://helparchive.huntertur.net/document/106771
- John McGowan, historical AVI codec overview:
  https://www.jmcgowan.com/avicodecs.html
- Toshiba Windows 98 SE OEM system.ini driver registration:
  https://support.dynabook.com/support/viewContentDetail?contentId=108859
- Microsoft installable-driver entry point (DriverProc):
  https://learn.microsoft.com/en-us/windows/win32/multimedia/installable-driver-format
