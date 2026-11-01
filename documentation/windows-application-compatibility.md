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
