# PNGFILT.DLL: Win98 FE/IE PNG image decode filter

State: **SOURCE IMPLEMENTED (COM/HEADER ONLY)**. PNG pixels are NOT
decoded, IE registration is NOT enabled, and tests are NOT run.

## Historical evidence

- Microsoft KB197017 lists Windows 98 FE `Pngfilt.dll` as
  **57,344 bytes**, **version 4.72.3110.0**, timestamp
  **1998-05-11 20:01**.
  https://jeffpar.github.io/kbarchive/kb/197/Q197017/
- Windows 98 FE DMF floppy KB191057 lists **57,344 bytes** in
  **WIN98_32.CAB** (disk 27), with the separate May 1 source
  timestamp. Keep package/install dates distinct.
  https://helparchive.huntertur.net/document/106792
- Microsoft IE5.5 KB265092 lists a later **44,304-byte**
  `Pngfilt.dll` version **5.50.4134.600** (June 2000) in
  `ADVAUTH.CAB`. The 1998 and 2000 versions are not assumed
  binary-compatible without testing.
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/265092
- Microsoft MS05-025 documents a critical PNG parsing vulnerability
  CAN-2005-1211 in IE's PNG rendering. Never copy unsafe old code.
  https://learn.microsoft.com/en-us/security-updates/securitybulletins/2005/ms05-025

## API boundaries and evidence

Microsoft documents an IE image filter COM contract
`IImageDecodeFilter` IID
`{A3CCEDF3-2DE2-11D0-86F4-00A0C913F750}`:
`Initialize(IImageDecodeEventSink *)`, `Process(IStream *)`
and `Terminate(HRESULT)`. The stream can be **sequential-only**.
https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa753599(v=vs.85)

Later PNG MIME registration examples name class
`CoPNGFilter` CLSID
`{A3CCEDF7-2DE2-11D0-86F4-00A0C913F750}`.
The exact Windows 98 FE registration and **native PE export table**
remain unverified against a real binary.
https://superuser.com/questions/406434/ie8-wont-display-png-images

This IE filter is **not** Windows Search IFilter, WIC or
libpng. Water already provides PNG decoding via the much later
`windowscodecs.dll` module, plus `gdiplus.dll`, `mshtml.dll`
and `urlmon.dll`. WIC is **not a Win98 FE guest ABI**.

## First-stage Water implementation

`dlls/pngfilt` now exports standard COM DLL entry points, provides
a reference-counted class factory and an `IImageDecodeFilter`
instance, and tracks outstanding objects and server locks.

`Process` reads exactly a bounded 33-byte PNG header without
seeking, checks PNG signature, 13-byte IHDR, CRC32, image dimension
bounds, bit depths and color types. A malformed header gets an error.
A valid header still returns **E_NOTIMPL**, because no raster data
is inflated/unfiltered and no output surface is supplied to IE.

`DllRegisterServer` intentionally returns `SELFREG_E_CLASS`:
installing this incomplete handler must **not** disable PNG on a
system with a working native IE filter. `DllUnregisterServer`
is a no-op; no registry keys are created. Factory/parse tests
are included but **not yet executed**.

## Missing capability gates

1. Acquire a licensed original FE binary + installation metadata.
   Record PE exports, CLSID/IIDs, version, SHA-256 and MIME keys.
2. Define and test `IImageDecodeEventSink` pixel surfaces/events,
   including palette, transparency, progressive/interlaced updates.
3. Connect a secure decoder to the sink without requiring modern
   WIC calls in a Win98 guest. Ensure CRC/truncation/overflow tests.
4. Test in Water Win32 x86 (PE32) and an IE4-era installation,
   then enable registration **only** after genuine rendering works.
5. Compare version-specific behavior with IE5.5/IE6 without
   overwriting the 1998 evidence.
