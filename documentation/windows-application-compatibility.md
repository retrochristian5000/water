# Windows application compatibility and software provenance ledger

Status: HISTORICAL SOURCES CHECKED; WATER WIN16 PATHS INSPECTED;
ORIGINAL PROGRAM MEDIA AND INSTALLERS NOT EXECUTED; NO PROGRAM-SPECIFIC
RUNTIME FIX CLAIMED. Review: 2026-10-23.

This is a cross-topic diagnostic ledger, not a mandate to invent OS files.
Use it alongside `windows-native-pe-module-matrix.md`,
`win95rtm-hotfix-regressions.md`, `winini-parsing.md`,
`win31-386-enhanced.md` (the OS-supplied Win3.1 CPWIN386.CPL
virtual-memory and 32-bit disk applet), and the existing Win16/WinMM/NE
sources. Preserve their older datapoints.

## Two independent classification axes

**Role / owner** (what executes it or provides its API):
- OS core / bundled OS tool (e.g. Windows 3.1 WIN.COM, KERNEL, USER, GDI,
  MMSYSTEM); record **which** Windows release and whether it was installed.
- Optional OS extension / redistributable runtime (e.g. Win32s, WinG,
  Video for Windows on an OS where installed separately). Not necessarily
  included with an original base installation.
- Device driver (e.g. display or wave-audio driver); Windows-bundled,
  OEM-supplied and application-supplied drivers require separate provenance.
- Application (e.g. Microsoft Arcade, Microsoft Dangerous Creatures).
- Application-private support DLL, codec, font or data file (e.g. the
  WaveMix audio library used by Microsoft Arcade).
- Installation tool / utility. The installer and the installed binary
  are not automatically the same role.
- UNKNOWN where imports, file origin, installer registration or functionality
  cannot yet be verified.

**Distribution / lifecycle** (how it arrived):
- Original OS media; optional OS component; manufacturer preload;
  separate application media; app-bundled dependency; independently
  redistributable runtime; application update; OS hotfix; service pack;
  unverified.
- A *patch* or *update* is a lifecycle property, not proof of OS ownership.
  A Microsoft-signed program, a file copied into WINDOWS/SYSTEM, or a
  product included in an OEM bundle is not automatically part of Windows.
- A filename, copyright, version string, Windows API import or machine
  type by itself does not prove package membership or compatibility.

Keep the following alongside both axes: original product/release, vendor,
date, installation source, file path, byte size/hash when available,
NE/PE/DOS/VxD format, CPU minimum and mode, imported DLL/ordinal list,
entrypoints, config keys, license/provenance, confidence, and negative
evidence. Record **shipped on media**, **installed by Setup**, **enabled**,
and **actually loaded by the process** as four independent states.
Do not synthesize a manifest from guessed filenames.

## Initial reference applications

| Item | Role | Distribution state | OS / hardware evidence | Verification boundary |
| --- | --- | --- | --- | --- |
| Microsoft Arcade, 1993 | Win16 game suite, five titles | Separate 3.5-inch disk release | Windows 3.1 or later; catalog lists 286 minimum | No original executable imports or live run inspected |
| WaveMix used by Arcade | Application-support audio library | Game audio dependency; provenance per setup media still to extract | Pre-DirectX digital sound mixing | Do not promote to DirectMusic, core WinMM or an OS DLL |
| WAVEMIX.EXE, 1998-05-07 | Installer for Arcade-specific sound fix | Application update for problems under Windows 95/98 | Archived Microsoft KB Q169430 | Does not imply a Windows 98 service pack or changed OS API |
| Microsoft Dangerous Creatures 1.0, 1994 | Microsoft Home multimedia reference application; ID `DC-1994-APP` | Separate CD-ROM; OEM bundled copies may also exist | Windows 3.1+, 386SX+, 4 MB RAM, CD-ROM, sound board, VGA+ | No actual binary imports, media codec or installer file list inspected |
| Dangerous Creatures (256 color), 1995 | Desktop-theme package, **not the 1994 application**; ID `DC-PLUS95-THEME` | Microsoft Plus! for Windows 95 add-on, originally separate from the OS | Wallpapers, cursors, sounds, desktop icons, screen saver | Original package inventory and file hashes not inspected |
| Dangerous Creatures (256 color), Windows 98 | Optional Windows 98 Desktop Themes content; ID `DC-WIN98-THEME` | Included with Windows 98 Desktop Themes after Plus! for Windows 95; distinguish **on media** from **selected for installation** | Microsoft Q178690 explicitly identifies its screen saver as a Plus! for Windows 95 carryover included with Windows 98 Desktop Themes | Original 98 FE/SE media manifest, selected-setup state, installed assets and hashes unverified |
| MSDANGER.INI | `DC-1994-APP` application config | The 1994 README locates it in the Windows directory | `VideoInWindow=0` versus `=1` | Path does not imply OS ownership; not the desktop theme |
| Windows 3.1 SYSTEM.INI | OS/driver configuration | Installed OS config, possibly changed by third parties | `[386Enh]` EMMEXCLUDE and VIRTUALHDIRQ mentioned by game README | An application's troubleshooting instructions do not turn it into an OS patch |
| Win32s 1.30c | Optional Win32-on-Win3.1 compatibility runtime | Separately distributed upgrade (KB Q122235) | Win32 apps under Windows 3.1/WfW | No evidence either listed application requires Win32s |
| MCIAVI / Video for Windows | Multimedia platform/runtime candidate | Installed availability must be checked per OS/media | MCI drivers use SYSTEM.INI [mci] or registry | A video clip does not prove the app's exact codec, API or imported DLL |
| Windows display/sound drivers | Device-specific OS-adjacent components | OS- or hardware-vendor-supplied | Dangerous Creatures README documents display and sound-card problems | Do not merge with the game executable |

These classifications describe the named products or documented software
functions, **not** the complete file manifests of original installation media.
The 286 catalog value for Arcade is less direct evidence than a scanned
original requirements card and needs confirmation; it must not be copied
into Dangerous Creatures' documented 386SX requirements.

## Same-name collision guard: Dangerous Creatures

The literal name **Dangerous Creatures** is ambiguous in the 1990s
Microsoft catalog. Preserve three separate evidence identities:

1. `DC-1994-APP`: the 1994 multimedia encyclopedia for Windows 3.1,
   on separately distributed application CD-ROM media.
2. `DC-PLUS95-THEME`: the **Dangerous Creatures (256 color)**
   desktop theme distributed with Microsoft Plus! for Windows 95.
3. `DC-WIN98-THEME`: the themed assets included among **Windows 98
   Desktop Themes**. They descend from the Plus! for Windows 95 theme,
   not necessarily the new themes introduced with the *separate*
   Microsoft Plus! 98 add-on. Do not overwrite the 1995 identity.

Microsoft KB Q178690 is unusually explicit: the original Plus! for
Windows 95 **Dangerous Creatures screen saver** is also included with
Windows 98 Desktop Themes and has a single-primary-monitor limitation
in multi-monitor use. KB Q187981 shows that Windows 98 Desktop Themes
can be added or removed separately from the Plus! 98 installation.
These sources establish distribution/component ancestry, **not**
installation on a clean Windows 98 machine or binary byte identity.

**Do not merge by title, author, dates, animal illustrations, or
Microsoft branding.** The 1994 application is tested via its NE EXE,
CD-ROM content, multimedia imports and `MSDANGER.INI`. The desktop
theme is tested via a `.theme` configuration and *actual listed*
wallpaper, icon, cursor, sound and screen-saver dependencies; no exact
original filenames, exports or resource payloads are asserted until
the appropriate setup CABs/install media are inspected. Treat the
theme's `.SCR`/possible companion DLL as installed accessories, not
as encyclopedia binaries. Do not infer DirectMusic or the encyclopedia
from shared sound/animal motifs.

Historical package / setup audit for `DC-WIN98-THEME`:
- Compare original Plus! for Windows 95 media with Windows 98 FE and
  SE CAB/INF manifests, recording actual file names, sizes, hashes,
  version and the feature/INF responsible for installation.
- In an unmodified disposable Windows 98 guest, record theme content
  *on media*, the Desktop Themes feature checked/unchecked during
  Setup, files actually installed, and theme selected/active status.
- Inspect original `.theme` references and invoke Windows 98-era
  Desktop Themes functionality; verify assets independently (cursor,
  wallpaper, desktop sounds, icons, screen saver).
- Probe the Q178690 multi-monitor case only when the relevant theme
  screen saver is installed. Failure on a secondary monitor is
  historical expected behavior for that screen saver, not necessarily
  a Water GDI defect.
- Confirm the capability owner and compatibility layer **before**
  changing `dlls/desk.cpl` or other GUI code. Water's
  `dlls/uxtheme` is Win32 5.1 visual-style code, **not** a validated
  implementation of Windows 98's classic Desktop Themes tool.
- Separately run `DC-1994-APP` as a Windows 3.1 multimedia app.
  Successful theme selection never counts as a passing application test.

This guard also applies to other product/title collisions: record
a provenance-qualified identity **before** aggregating hits or
opening a new feature ticket.

## Separate, reproducible application probes

### Microsoft Arcade (Windows 3.1 baseline)

1. Capture product/distribution revision, installer listing, and file hashes
   from lawfully obtained media; retain original abbreviated `.??_` names.
2. Determine executable format, imported NE modules/ordinals, bundled DLLs
   and WaveMix package identity. Test native application DLL loading before
   writing a replacement.
3. Verify five titles separately: launch, controls, GDI/paint/palette,
   waveform mixing and completion callbacks, configuration, and help.
4. Compare original Windows 3.1 with Windows 95/98 behavior only after
   recording which WaveMix/INI revision is installed. Treat the May 1998
   WAVEMIX.EXE as an **application** fix; verify original vs patched files.
5. Attribute every observed failure to loader / GDI / USER / MMSYSTEM16 /
   WinMM / application library / installer / environment **only after**
   tracing the failed API or module. No DirectMusic dependency assumed.

### Microsoft Dangerous Creatures (Windows 3.1 multimedia/CD baseline)

1. Capture the CD-ROM's original directory layout, setup actions,
   extracted files, hashes and imports. Verify drive-letter and media-
   presence behavior; do not hide the missing disc by fabricating files.
2. Verify the first-run overview movie, audio narration, image pages,
   searching/index, printing, and interactive navigation. Record independent
   successes and failures.
3. Compare 16-color and 256-color behavior, palette ownership, full-screen
   vs in-window movie presentation, and `MSDANGER.INI`'s `VideoInWindow`
   key. Identify actual video container/codec only from media inspection.
4. Probe low memory and Windows 3.1 execution mode. The original README
   suggests `WIN /S` and `WIN /D:XV` for display troubleshooting.
   Those are OS-launcher/mode/debug options; do not implement them inside
   Dangerous Creatures or conflate them with `WIN.COM`'s NT-style shim.
5. Confirm CD-ROM latency and read errors separately from sound-card
   interrupt/DMA or AVI/MCI codec errors. The README's MPC performance,
   hardware drivers, 4 MB minimum, and 16/256-color expectations are
   historical *test inputs*, not a promise of modern host-device support.
6. The README documents Ctrl+Alt+D demonstration/museum mode. Treat this
   as **application-specific input**, not a global Windows shortcut.

### Test result format

```text
Product/release | media ID/hash | guest OS/personality and build SHA |
minimum CPU and mode | probe (specific action) | expected behavior |
actual behavior | failing module/API/ordinal or log | classification |
evidence source and confidence | pass/fail/blocked/not-run
```

Mark every probe NOT RUN until the installer or application is actually
executed on a recorded Water build. Passing a source inspection, compiling
the Win16 bridge, or finding a filename does not count as application
compatibility. Use a disposable guest/prefix and avoid installing
historical drivers into an actual host OS.

## Water implementation ownership (existing tree, not new work)

- `dlls/krnl386.exe16/ne_module.c`: native NE library loading,
  dependencies/entrypoints. Do not substitute native 64-bit PE binaries.
- `dlls/mmsystem.dll16/mmsystem.c` and `message16.c`: Win16
  waveform and MIDI bridge, segmented WAVEHDR and callback marshalling.
- `dlls/winmm/waveform.c`: host-side wave playback dispatch.
- `dlls/gdi.exe16` / USER16: game drawing, color/palette and interaction.
- `documentation/winini-parsing.md`: already-audited private profile APIs
  needed for application INI files; distinguish app's private INI from
  `SYSTEM.INI` OS/driver configuration.
- `documentation/windows-native-pe-module-matrix.md`: existing
  machine/OS-provenance controls; an app test must not bypass them.

Implement/fix a Water subsystem only after a failing application probe is
reduced to a supported Windows contract. Keep application-owned formats
and installation/update provenance outside OS component inventory.

## SVG chronology and separate implementation owners (November 1, 2026)

**Version and provenance must be separated from installed functionality.**
Scalable Vector Graphics (SVG) is an XML-based W3C vector format,
not a variant of BMP/RLE, WMF, EMF, .ICO, or a video codec.

| Historical evidence | Date | Correct interpretation |
| --- | --- | --- |
| W3C SVG draft series | 1999 AD | Experimental, draft-level tools and markup; no generic Windows 3.1/95 shell support |
| Adobe SVG Viewer 2.0 build 55 | March 14, 2001 AD | Optional browser plug-in / IE ActiveX viewer; release notes explicitly support Windows 95/98/SE/Me/2000 and NT 4 SP4+ |
| W3C SVG 1.0 Recommendation | September 4, 2001 AD | Standardized file format; not proof of bundled OS decoder |
| W3C SVG 1.1 Recommendation | January 14, 2003 AD | Later language version, not a Windows 98 SE built-in graphics upgrade |
| Internet Explorer 9 | March 15, 2011 AD | Native SVG 1.1 browser document-mode support in IE9 mode; Microsoft explicitly distinguishes IE9+ from older IE implementations |

The Adobe **2001 release notes are unusually strong evidence**:
they list Windows 95/98/SE/Me and IE 4.0+ or Netscape 4.0-4.75.
They explicitly describe SVG 2.0 Viewer as implementing much of
the November 2, 2000 W3C *candidate* SVG specification. Therefore
do **not** retroactively assign every SVG 1.1 or later feature to
Adobe 2.0. Native IE9 SVG support does not mean that IE9 runs on
Windows 9x; it was a separate later browser/OS generation.

### WATER tree ownership

- **Build-time SVG assets, not guest runtime:** `configure.ac`
  checks for `rsvg-convert`/`rsvg` only when maintainer mode is
  selected. `dlls/ieframe/ietoolbar.svg` is an SVG 1.1 source
  graphic listed in that module's Makefile. Rebuilding prepackaged
  icons must not accidentally require a runtime guest SVG decoder
  or rewrite Windows 95 shell icon APIs.
- **Web document SVG:** `dlls/mshtml/svg.c` contains COM wrappers
  for selected Gecko SVG element interfaces (SVGSVGElement,
  SVGCircleElement, SVGTextContentElement). Many SVG methods still
  return `E_NOTIMPL`, so source presence alone is **not proof**
  of full native SVG 1.1/2 rendering or scripting support.
  `dlls/mshtml/htmldoc.c` handles `image/svg+xml` as a distinct
  MIME type rather than folding it into generic XML.
- **Historical browser-mode guard, repaired:** `HTMLElement_Create`
  in `dlls/mshtml/htmlelem.c` previously created SVG-specific
  COM element wrappers whenever Gecko returned an SVG interface,
  even for pre-IE9 emulated document modes. It now selects these
  wrappers only at `COMPAT_MODE_IE9` or newer. Older document
  modes take the existing generic non-HTML element path. This
  controls the **COM DOM surface**; it does **not** deactivate
  Gecko's underlying SVG rendering or emulate an actual Adobe
  ActiveX SVG viewer. Browser engine MIME/rendering behavior still
  needs a separate version-aware audit.
- **No inferred wallpaper/OS SVG:** `dlls/user32/desktop.c`
  uses `LoadImageW(IMAGE_BITMAP)` with an OLE picture fallback,
  not the MSHTML SVG DOM; `dlls/gdiplus/image.c` lists raster
  and EMF/WMF/icon codec paths, not a verified SVG decoder.
  SVG should not be admitted as native Windows 95/98 wallpaper
  just because modern Wine builds include SVG graphics assets.
  Keep `documentation/winini-parsing.md` wallpaper and
  `documentation/win95-rle-graphics.md` RLE paths independent.

### Regression/negative controls

1. IE7/IE8 document-mode SVG nodes must not expose the IE9+ native
   `ISVGElement`/subtype wrappers merely because Gecko supports SVG.
2. IE9+ retains its existing wrapper creation, reference release,
   and error/fallback paths. Mark individual unimplemented methods
   `E_NOTIMPL`; do not claim complete SVG DOM conformance.
3. Adobe SVG Viewer is **optional external application/plugin**
   support, not default Windows 95/98/Me OS support. Distinguish
   installed plug-in registration from browser-native behavior.
4. SVG build sources may generate bitmap/ICO artifacts, but this
   must not make Win9x runtime depend on `rsvg`, GDI RLE, or WIC.
5. Native SVG wallpaper and complete browser execution remain
   **NOT TESTED** until genuine media and runtime probes verify them.

No complete `mshtml` build, browser-mode execution, Adobe Viewer
installation, or 9x guest SVG-file test is claimed.

Primary/contemporary references:
- W3C official SVG 1.0 Recommendation history:
  https://www.w3.org/standards/history/SVG/
- W3C official SVG 1.1 history:
  https://www.w3.org/standards/history/SVG11/
- Adobe SVG Viewer 2.0, build 55, dated March 14, 2001, shipped README:
  https://st1.asflib.net/MEDIA/ASF-CD/ASF-M-00161/ACROREAD/INSTALLER/SVG_FILES/README.HTML
- Microsoft SVG implementation modes (IE9+):
  https://learn.microsoft.com/en-us/openspecs/ie_standards/ms-svg/6c6d7a7b-727e-403a-9e39-9db96f2b5353
- Microsoft IE9 release date:
  https://learn.microsoft.com/en-us/lifecycle/products/internet-explorer-9

## Sources and confidence

**Contemporary Microsoft primary documentation, available as KB mirrors:**
- Microsoft Arcade original README, KB Q104584 (1993):
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/104584
- Microsoft Dangerous Creatures original README, June 24, 1994,
  reproduced as KB Q118407:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/118407
- Microsoft Arcade WaveMix update, KB Q169430; May 7, 1998 release:
  https://jeffpar.github.io/kbarchive/kb/169/Q169430/
- Microsoft Win32s upgrade, KB Q122235:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/122235
- Microsoft MCI driver configuration (SYSTEM.INI vs registry):
  https://learn.microsoft.com/en-us/windows/win32/multimedia/device-names
- Microsoft KB Q178690, *Screen Saver Is Displayed Only on Primary
  Monitor*: identifies Dangerous Creatures as originally in Plus! for
  Windows 95 and included with Windows 98 Desktop Themes:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/178690
- Microsoft KB Q187981, *Plus! 98: Desktop Themes Tool or Features
  Missing*: distinguishes Windows 98 desktop themes from Plus! 98:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/187981
- Microsoft Plus! 98 release notice, 1998-06-25; identifies Plus! 98
  as a separate Windows 98 companion product with additional themes:
  https://news.microsoft.com/source/1998/06/25/microsoft-announces-immediate-availability-of-microsoft-plus-98/

**Independent catalogs, not substitutes for original media:**
- https://winworldpc.com/product/microsoft-arcade
- https://winworldpc.com/product/microsoft-dangerous-creatures/10
- https://www.acmi.net.au/works/84243--dangerous-creatures-pc/

No original disc images, theme CAB/INF payloads, program binaries, actual
import tables or hardware drivers were inspected for this ledger. The sources
establish historical requirements, product identities and selected
user-facing behavior only. Theme distribution evidence does not establish
installation in any particular Windows 98 guest.

## Voxware codec provenance and IE-era media boundary (November 1, 2026)

**State: source and historical audit; by-name ACM registration repaired;
Voxware decoding NOT IMPLEMENTED or runtime-verified.** This is a media
compatibility topic, not an FTP folder or HTML rendering problem.
Internet Explorer could host media controls that used Windows/NetShow/
Windows Media Player codecs, but the presence of an IE executable is
not proof that any particular third-party codec is installed.

### Source-backed product and distribution constraints

- Microsoft KB Q141483 names Voxware (`VOX`) speech compression for
  *Bookshelf 1996–97* pronunciations. The article explains that
  multimedia CDs can supply drivers not normally included with Windows:
  https://jeffpar.github.io/kbarchive/kb/141/Q141483/
- Microsoft's **7 April 1997** NetShow 2.0 announcement identifies
  Voxware **MetaVoice** and **MetaSound**, optimized for low-bandwidth
  speech and audio respectively. Availability within NetShow does not
  prove inclusion in a clean Windows OS install:
  https://news.microsoft.com/source/1997/04/07/new-compression-technology-added-to-netshow-2-0-provides-broadcasters-with-widest-range-of-high-quality-codecs/
- Microsoft KB Q175757 (MSN 2.51, last reviewed 27 October 1997) lists
  `Voxplay.exe`, version **2.5.13.39**, under components installed by
  **Typical MSN Setup**; an IE4 installer was separately listed, while
  a NetShow Player CAB was listed with *Full* MSN installation. Treat
  these as **MSN setup components**, not as proof that Windows 95 or
  Internet Explorer 4 installed Voxware by default:
  https://jeffpar.github.io/kbarchive/kb/175/Q175757/
- Microsoft KB Q168369 documents a *Netscape ACM wrapper for Voxware
  RT24 speech* conflict with Microsoft Plus! for Kids on Windows 95/98.
  A third-party browser codec may affect OS audio without becoming an
  OS-distributed codec:
  https://jeffpar.github.io/kbarchive/kb/168/Q168369/
- Microsoft KB884769 (last modified 5 August 2005) identifies unsigned
  legacy codec packages for MetaSound and MetaVoice. Their availability
  from a codec server was not equivalent to having them installed.
  Its IE unsigned-ActiveX workaround carries an explicit security
  warning: **do not lower security or auto-install obsolete CABs**
  just to simulate historical behavior:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/884769
- A **February 2005** Windows 98 SE / IE6 SP1 / WMP9 user report
  identifies a `voxacm.cab` package containing `msms001.vwp`,
  `mvoice.vwp`, `vct3216.acm`, `vct3216.dll`,
  `voxacm.inf`, `voxmsdec.ax`, and `voxmvdec.ax`.
  This is a reported package inventory, **not** a verified original
  clean-install media manifest or permission to redistribute binaries:
  https://www.dell.com/community/en/conversations/windows-general/cant-view-video-tutorials-from-the-dell-website/647e52a5f4ccf8a8de326226

### Format-ID and ABI boundaries

RFC 2361, June 1998, gives the relevant WAVE format tags:
`0x0074` = Voxware RT24 / MetaVoice;
`0x0075` = Voxware RT29 / MetaSound; and `0x0082` = Microsoft
MSRT24, the alternate ACM tag referring to MetaVoice. Older Voxware
tags also exist and must **not** be flattened into one decoder.
Water already declares `WAVE_FORMAT_VOXWARE_RT24`,
`WAVE_FORMAT_VOXWARE_RT29`, `WAVE_FORMAT_MSRT24`,
and related legacy tags in `include/mmreg.h`:
https://www.rfc-editor.org/rfc/rfc2361

- `dlls/msacm32/internal.c` discovers `msacm.*` driver values in
  Drivers32 and the system.ini driver section. Its
  `MSACM_RegisterDriverFromRegistry` previously **discarded** the
  pointer returned by `MSACM_RegisterDriver`, so
  `acmDriverAddW(... ACM_DRIVERADDF_NAME)` in
  `dlls/msacm32/driver.c` incorrectly reported an error even when
  registration succeeded. The November 1 change returns the actual
  pointer; this fixes a generic third-party ACM registration contract,
  **not** the Voxware codec algorithm.
- `dlls/quartz/acmwrapper.c` already calls `acmStreamOpen` for
  non-PCM WAVEFORMATEX audio. For installed/compatible ACM drivers,
  codec selection belongs to ACM, not `programs/iexplore`.
  An uninstalled decoder must not be advertised as present.
- `dlls/wmp/player.c`, `dlls/wmvcore`, and
  `dlls/winegstreamer` contain separate player, ASF, and backend
  pathways. Their ability to play a particular Voxware stream is not
  established by the ACM registration fix.
- FFmpeg has a standalone open-source **MetaSound** decoder and its
  RIFF mapping identifies `0x0075`. This is a promising independent
  compatibility reference, not proof that Water's GStreamer/ACM
  pipeline currently decodes MetaSound or MetaVoice. License,
  packetization, sample format and bitstream test vectors must be
  checked before integrating any decoder:
  https://ffmpeg.org/doxygen/trunk/metasound_8c_source.html
  https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/riff.c

### Reproduction and implementation criteria

1. Record original Windows 95, Windows 98 FE/SE, Windows Me, Windows
   2000 and XP installation media separately. For each, capture
   *on media*, *Setup selected*, *installed*, and *loaded* states.
   Overlay IE, MSN, NetShow, WMP and application installers as distinct
   provenance layers. No clean-OS bundled-Voxware claim is yet locked.
2. Inspect lawful media and extracted INF/ACM/AX/VWP metadata in an
   offline/disposable guest, recording hashes, machine format
   (Win16/Win32), registrations, CLSIDs, format tag and installed
   dependencies. Avoid executing legacy unsigned binaries on the host.
3. In `msacm32`, exercise `acmDriverAddW` by name with a disposable
   registered test driver and assert a real driver ID on success and
   failure on absent names, without global host-registry modification.
   Then test an actually installed Voxware ACM driver separately.
4. For `0x0075` MetaSound, establish a real decode route (native
   clean-room/LGPL-compatible decoder or verified host GStreamer codec),
   distinct from `0x0074` MetaVoice and `0x0082` MSRT24. Test
   RIFF/WAV and ASF streams separately with documented golden PCM
   outputs, headers, channel modes, block boundaries, seeking,
   truncated inputs and errors.
5. Exercise WMP and IE-embedded Windows Media playback with a
   documented control/plug-in installation, checking the graph built
   by DirectShow/ACM. Do **not** synthesize an ActiveX codec download
   prompt or bypass IE's zone security to paper over missing codecs.
6. Validate 16-/32-bit binary and calling-convention boundaries and
   compare per supported Windows personality. A successful codec tag
   lookup or registration is not a successful decoder test.

**Verification:** historical references and repository paths inspected;
single-line by-name driver ID repair reviewed at source level only.
No Windows-guest Voxware binary, decoder, player stream, build or
runtime regression has been validated in this audit.

## IEFRAME export parity audit (November 1, 2026)

**Version boundary:** IEFRAME.DLL was introduced with **Internet Explorer 7**
(2006) and bundled with **Windows Vista**. IE7/IE8 installations on older
Windows NT systems can supply IEFRAME as an *IE upgrade*, not as a
component of the untouched Windows base. IE6 and earlier rely on older
SHDOCVW/BROWSEUI ownership. Never set IEFRAME present merely from the
OS personality name.

Historical export evidence:
- Geoff Chappell, IEFRAME history, native 7.0 versus 8.0 distributions:
  https://geoffchappell.com/studies/windows/ie/ieframe/history/index.htm
- IEFRAME 7.0 named exports and ordinal-only exports:
  https://geoffchappell.com/studies/windows/ie/ieframe/history/names700.htm
  https://geoffchappell.com/studies/windows/ie/ieframe/history/ords700.htm
- IEFRAME 8.0 additional named exports:
  https://geoffchappell.com/studies/windows/ie/ieframe/history/names800.htm
- Microsoft's DllGetVersion and DLLVERSIONINFO contract:
  https://learn.microsoft.com/en-us/windows/win32/shell/versions
  https://learn.microsoft.com/en-us/windows/win32/api/shlwapi/ns-shlwapi-dllversioninfo

### Source comparison and current boundaries

At the start of this audit, `dlls/ieframe/ieframe.spec` exposed
**9 entries**: `IEWinMain` (ordinal 101), plus eight named exports.
The real IEFRAME 7.0/8.0 DLLs provide many more; compare actual
export tables **by target version, bitness, ordinal and name** before
adding anything. Avoid claiming a Windows 7 IE8 export was shipped
with unmodified XP IE6 or Windows 98.

| Native export / function | Earliest relevant IEFRAME | Water disposition |
| --- | --- | --- |
| `DllGetVersion` | IE7 | **Implemented in this patch**, using module version resources; `DLLVERSIONINFO` and `DLLVERSIONINFO2` |
| `DllInstall` | IE7 | Missing in IEFRAME export table; SHDOCVW's own `DllInstall` is only a success-returning stub |
| `DoFileDownload`, `DoAddToFavDlg`, `DoOrganizeFavDlgW` | IE7; earlier SHDOCVW | Missing from IEFRAME; inspect existing SHDOCVW implementations and native forwarding policy |
| `IEParseDisplayNameWithBCW`, ordinal 218 | IE7; earlier SHDOCVW | Missing from IEFRAME; SHDOCVW ordinal 218 presently returns `E_FAIL`; this is the separate FTP/URL Shell PIDL blocker |
| `IEIsProtectedModeProcess`, `IEIsProtectedModeURL` | IE7 | Missing; security / integrity-level behavior must not be faked |
| `IEGetWriteableHKCU` | IE7 | Exported but returns `E_NOTIMPL`; no raw-HKCU shortcut may bypass Protected Mode |
| `IERefreshElevationPolicy` | IE7 on Vista SP1+ | Exported; version-restricted behavior to verify |
| `IECreateFile`, `IEDeleteFile`, `IEFindFirstFile` | IE8 | Missing; use Protected Mode-aware semantics, not plain filesystem pass-through |

A version-resource correction also defines `VER_FILEVERSION` alongside
WATER's existing IE8 `VER_PRODUCTVERSION`. `DllGetVersion` reports
its own module file version (not `GetVersionEx`, host OS version, or a
guessed IE update level), returning `E_POINTER` / `E_INVALIDARG`
for invalid output/size. The export test in
`dlls/ieframe/tests/ie.c` loads the actual module and checks the
named export and both version-info structures, skipping clean
personalities without IEFRAME.

**Verification limit:** source audit and committed test only. The
runtime ABI/export list must still be compared against genuine IE7 and
IE8 32-bit and 64-bit binaries; compiled build, test run and actual
Internet Explorer application behavior were not validated here.
Export coverage must not be inflated by fake success stubs.

### InternetShortcut optional icon metadata

The existing `dlls/ieframe/intshcut.c` implementation of
`IPersistFile::Save` writes the mandatory `[InternetShortcut]` /
`URL=` contents and then reopens the optional `FMTID_Intshcut`
property storage to retrieve icon properties. If that second
`IPropertySetStorage_Open` or subsequent
`IPropertyStorage_ReadMultiple` failed, the method returned the
metadata error even though the .url contents were already written.
The read-failure branch also failed to release the storage interface.

The November 1 repair makes those *optional metadata* failures
nonfatal and releases the successfully opened property storage,
without changing failure reporting when `CreateFileW` fails.
`dlls/ieframe/tests/intshcut.c` already has a test that saves a URL
before setting icon properties; this is an existing regression control,
not a newly executed test. The current source still does not check
each `WriteFile` return/byte count, so complete disk-write error
handling **remains unverified and is not claimed fixed**. This is
independent of IEFRAME DLL export presence.

Related contemporary independent reproduction (Wine 11.20; not a
Water runtime test):
https://list.winehq.org/hyperkitty/list/wine-bugs%40list.winehq.org/thread/MMXPYR35ZS5D656SYBK3UCT7WPPKLU6V/

## Browser Helper Objects — IE host compatibility audit (November 1, 2026)

Browser Helper Objects (BHOs) were supported in Internet Explorer 4 onward.
The BHO *hosting mechanism* must not be confused with the separate
installation of any extension (or with the later IEFRAME.DLL module).
Internet Explorer and Windows Explorer may each host BHOs, but their
site objects, window lifetimes, and NoExplorer filtering differ.

WATER previously had no BHO registry enumeration in its top-level IE
frame implementation, and IWebBrowser2::Quit returned E_NOTIMPL.
The new implementation in dlls/ieframe/bho.c (called from iexplore.c)
enumerates CLSID
subkeys under HKLM\Software\Microsoft\Windows\CurrentVersion\Explorer\
Browser Helper Objects, creates in-process COM IObjectWithSite objects,
passes its own IWebBrowser2 object to SetSite, and stores successfully
sited instances for SetSite(NULL) and Release during close and Quit.
It does NOT automatically load BHOs in the embeddable WebBrowser
ActiveX control or in programs/explorer.

Security and compatibility guards checked before CoCreateInstance:
- The explicit Enable Browser Extensions = No user preference and
  machine/user IE policy disable.
- The IE add-on policy list: CLSID deny entries, RestrictToList when
  not explicitly allowed, and per-user Ext\Settings CLSID Flags bit 0
  indicating disabled in IE add-on management.
- A missing/malformed CLSID, failed COM activation or absent
  IObjectWithSite cannot abort loading other entries.
- No registry mutation or bundled native BHO DLL is introduced.

References:
https://techcommunity.microsoft.com/blog/askperf/the-basics-of-browser-helper-objects/372599
https://learn.microsoft.com/en-us/windows/win32/api/ocidl/nf-ocidl-iobjectwithsite-setsite
https://mskb.pkisolutions.com/kb/298931
https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-gppref/8b0fcf80-3401-40cf-b4c5-c086238c7cb1
https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-internetexplorer
https://www.winhelponline.com/xp/addons.htm

Reentrant teardown is guarded: the callback to SetSite(NULL) can trigger
another Quit, so only the outermost detachment is permitted to walk the
per-browser BHO list, which is drained one entry at a time.

The BHO registry, policy and COM helper ownership is isolated in
bho.c rather than enlarging the top-level window manager with
extension policy code. The existing IE host and COM object retain
the lifecycle entry points.

Verification still needed: compiled and guest-runtime tests on a clean
IE4/IE5/IE6 and Vista IE7+ baseline, 32/64-bit registration views,
extension installation identity, benign disposable BHO mock with
SetSite(non-NULL)/SetSite(NULL) assertions, event subscription,
navigation, Quit/reentrancy and reference-cycle checks. The added
IEFRAME test checks that IWebBrowser2::Quit succeeds and hides the
browser, not complete BHO functionality. IE Protected Mode, Enhanced
Protected Mode, crash isolation, add-on management UI, kill bits,
COM bitness isolation, Explorer-specific host support, and complete
admin policy precedence remain separate work. Never claim native
security parity or load untrusted historical extensions on the host.

## Internet Explorer registry audit — homepage, security zones and BHO policy (November 1, 2026)

Status: SOURCE REPAIRED, TEST SOURCE ADDED, NOT COMPILED OR RUNTIME VERIFIED.
Continue the prior IEFRAME exports, BHO host, FTP Shell namespace and Voxware media audits; no reset.

### IEFRAME Home navigation

Microsoft's 2004 Start Page documentation establishes the normal user homepage under
HKCU\Software\Microsoft\Internet Explorer\Main (REG_SZ, Start Page).
Beginning with IE7, the policy homepage can also be specified by Start Page under
HKCU\Software\Policies\Microsoft\Internet Explorer\Main.
The HomePage control-panel policy does not itself contain the URL.
References:
https://devblogs.microsoft.com/scripting/how-can-i-change-the-internet-explorer-home-page/
https://mskb.pkisolutions.com/kb/2501278
https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-gppref/8b0fcf80-3401-40cf-b4c5-c086238c7cb1

The former dlls/ieframe/navigate.c go_home read only the ordinary user value,
used MAX_PATH for the URL and did not ensure null termination after RegQueryValueExW.
Microsoft explicitly warns that REG_SZ data returned by this API may not be
terminated. The repair checks IE7+ policy first and the normal user value next;
it validates type, byte length, capacity and null termination before navigating,
using INTERNET_MAX_URL_LENGTH as the bound and about:blank as the safe fallback.
https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexw

### URLMON protocol-to-security-zone registry

Microsoft's ProtocolDefaults registry contract uses per-protocol REG_DWORD zone IDs
under both user and machine Internet Settings ZoneMap configuration:
Software\Microsoft\Windows\CurrentVersion\Internet Settings\ZoneMap\ProtocolDefaults.
Historically, the known base zones include Local Machine (0), Local Intranet (1),
Trusted (2), Internet (3), and Restricted (4). The value represents a security
decision, so a missing profile key must not produce a spurious E_UNEXPECTED
or bypass a valid machine-wide default.
https://www.microsoft.com/en-us/msrc/blog/2008/03/ms08-015-protocol-handler-and-its-default-security-zone
https://helparchive.huntertur.net/document/17029

The repaired dlls/urlmon/sec_mgr.c get_zone_from_reg now tries HKCU then HKLM,
validates REG_DWORD type and exact size (including when a value is zero),
and defaults to URLZONE_INTERNET if no valid protocol default is available.
No user/machine registry writes are introduced.

New read-only regression probe in dlls/urlmon/tests/sec_mgr.c:
test_missing_user_protocol_defaults. The probe operates only when the per-user
ProtocolDefaults key is absent and the machine HTTP DWORD exists; otherwise it
skips without altering any registry keys. It checks IInternetSecurityManager
MapUrlToZone against a deterministic .invalid host. Additional domain/host
overrides must be tested separately and could make this conditional probe
environment-dependent.

### BHO policy precedence and registry-string validation

The original BHO work created objects from registered CLSID subkeys and used
IObjectWithSite. A subsequent registry audit found two independent issues:
(a) it treated a user No as a final disable even when a configured administrator
policy explicitly enabled extensions; (b) it checked returned REG_SZ buffers
without placing a terminator at the actual returned data length.

The repaired dlls/ieframe/bho.c reads the machine extension policy, then
the user policy, then the ordinary user preference; recognized Yes/No
values follow this precedence. It also terminates per-CLSID text policies
at the returned size. This preserves explicit administrator disables
without taking an unrecognized value as permission to bypass a deny.
https://mskb.pkisolutions.com/kb/298931
https://learn.microsoft.com/en-us/windows/client-management/mdm/policy-csp-internetexplorer

### Boundary and regression matrix

- IE6 and earlier may interpret homepage policies differently; do not copy
  IE7+ policy resolution into Win9x SHDOCVW by assuming identical history.
- Test normal/policy Start Page precedence; values without a NUL;
  empty, wrong-type, oversized and absent values, and standard pages.
- Test HKCU ProtocolDefaults missing, value missing, wrong REG type,
  correctly typed zero, HKLM fallback, both keys absent and custom zones.
- Test the BHO Yes/No administrator policy precedence versus the user
  setting and the older per-CLSID disable and allow-list controls.
- Test the 32/64-bit redirected COM registry views and native IE7/IE8
  binaries; IEFRAME does not belong in an untouched Windows 98 install.
- Preserve IE Security Zone and ActiveX kill-bit enforcement; no compatibility
  measure should quietly weaken security policy.

Verification: Source edits and test code pushed on master; no local compiler,
native IE install, Windows guest runtime, or compiled test result in this pass.
The URLMON conditional test is a source-level regression guard, not a pass.
