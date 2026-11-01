# WIN.INI parsing ledger — Windows 9x and Win16

Status: QUOTE-BOUNDARY AND FALLBACK/PATH REPAIRS; WIN3 PROGMAN
LOAD/RUN DISPATCH WIRED; AOL-RELATED ISDN INSTALLER EVIDENCE LOGGED.
COMPLETE WIN3/95/98 GUEST BEHAVIOR NOT YET VERIFIED.

## Ownership and prior findings

- dlls/kernel32/profile.c is the shared INI parser and implements
  GetPrivateProfile*, WritePrivateProfile* and GetProfile* APIs.
- dlls/krnl386.exe16/file.c owns the KERNEL Win16 profile wrappers.
  GetProfileInt16, GetProfileString16 and WriteProfileString16 explicitly
  use win.ini; low-level parsing remains shared.
- dlls/krnl386.exe16/task.c consumes the [Compatibility] section.
  A prior repair already limits compatibility flags to matching
  NE programs with expected Windows version less than 0x030a.
  Do not reset or silently weaken this gate.
- SYSTEM.INI driver mappings and MSDOS.SYS boot configuration are
  distinct inputs, not interchangeable WIN.INI sections.
- The shared parser is **not** responsible for executing [windows] load/run.
  `programs/progman/main.c` now owns the Win3 Program Manager startup
  dispatch; Win95/98 Explorer startup, NT WOW and third-party shells are
  separate owners and must not be forced through Program Manager.

## Confirmed quote-copy defect and repair

The old PROFILE_CopyEntry helper applied the output bound before
removing matched quotes. A quoted three-character value such as
load="ABC" lost a valid final character in an exact-fit four-WCHAR
buffer. A zero-length value such as run="" could write to buffer[-1]
when the caller's destination capacity was one WCHAR.

The fix removes matching delimiters first, then bounds the copy by
output capacity minus one; it terminates safely for length one and
does not touch the output for nonpositive length. Unmatched quotes
remain literal. memmove also handles overlapping input/output safely.

## Regression testing

In dlls/kernel32/tests/profile.c, a temporary .ini fixture with
[windows] and [Compatibility] exercises the Win32 ANSI and Unicode
APIs without modifying a real WIN.INI file. The tests cover exact
fit, truncation, single quotes and guard bytes around small outputs.

A standalone reproduction of the patched helper passed GCC C99
compilation with -Wall -Wextra -Werror and ASan/UBSan, including an
empty quoted value with a one-character destination. Full kernel32
build, Wine test execution and Windows 98 FE comparison remain pending.

## Additional profile-path and fallback repairs (November 1, 2026)

In `dlls/kernel32/profile.c`, GetPrivateProfileStringW previously
formed `def_val - 1` for an empty default string and unconditionally
dereferenced a trim-allocation result; it now trims via bounded lengths,
handles the empty/all-space cases, and preserves the original default
if allocation fails. The earlier matched-quote fix remains intact.

The shared profile path resolver also checks the GetWindowsDirectoryW
and GetFullPathNameW lengths before composing into a MAX_PATH buffer.
These protections apply to generic profile APIs, not a special AOL
parser. Existing `dlls/kernel32/tests/profile.c` now also checks empty
and space-only defaults plus raw [windows] section quotes. Full binary
validation remains pending.

## Windows 3.x Program Manager startup semantics

Microsoft Windows 3.x uses `WIN.INI` `[windows] load=` and `run=`
as **independent** startup lists. Windows 3.1 also has a Program
Manager Startup group selected through `PROGMAN.INI [Settings] Startup=`;
a blank Startup setting suppresses that group without deleting
WIN.INI run/load settings. Program Manager is the owner of the Win3
dispatch; Microsoft's KB Q85476 warns that an alternate shell may
ignore both the WIN.INI lists and the Startup group.

The new `programs/progman/main.c` dispatch consumes the raw WIN.INI
section (preserving quotes lost by GetProfileString on an entirely
quoted value) and starts each space-delimited filename with WinExec:
- `load=`: `SW_SHOWMINNOACTIVE` (minimized without activation);
- `run=`: `SW_SHOWNORMAL` (normal launch).
This follows Microsoft KB Q69844. The Startup-group path remains
separate, with its existing Shift/empty-group suppression. Quoted
filenames can contain spaces; no unsupported argument grammar was
invented. Long/truncated section reads are not executed. This is
**not** evidence that WATER boots DOS Windows 3.x, or that the
Win95/98 Explorer shell now processes WIN.INI.

**Windows 3.1-specific boundary:** Microsoft Q81606 states that holding
Shift disables the Program Manager Startup group, not the independent
WIN.INI load/run lists. In Windows 95, the documented Shift suppression
also skips WIN.INI load/run. A universal startup-suppression flag would
lose that release distinction.

**Known Program Manager startup scheduling gap:** Microsoft Q91204
describes icon-row/column order for Windows programs before DOS programs,
with Windows command-line switches and the first DOS item having
special scheduling rules. WATER currently iterates the Startup group's
linked list in insertion order. Without reliable NE-versus-DOS
executable classification and icon-position sorting, this is **not
yet faithful Windows 3.1 startup scheduling**. Do not re-label all
`.EXE` icons as Windows tasks.

For full boot/stage ownership and the WfW 3.1/3.11 split, see
`documentation/win31-386-enhanced.md`. The historical
`SYSTEM.INI [boot] shell=` entry determines whether Program Manager
even runs. When an alternate shell owns startup, the installer-added
AOLISDN or OEMcFos WIN.INI entries below may be ignored despite being
present in the file (Microsoft Q85476).

## Desktop wallpaper: Windows 3.x versus later picture positioning

The Windows 3.1 Resource Kit and Microsoft KB Q83433 place these
settings under `WIN.INI [desktop]`:

- `Wallpaper=<bitmap-filename>`: original BMP filename (including a
  path if the image is outside the Windows or system directory).
- `TileWallpaper=0`: center at original size, clipping oversized
  images symmetrically rather than forcing them into the top-left.
- `TileWallpaper=1`: repeat at native size across the desktop.
- `Pattern=`: independent 8x8 background pattern, still visible
  around a centered image smaller than the desktop.

**Do not attribute Fill or Fit to Windows 3.1.** They are Windows 7+
picture-positioning modes. The later desktop registry uses
`HKCU\\Control Panel\\Desktop` REG_SZ `TileWallpaper` and
`WallpaperStyle` (center/tile: `0`; stretch: `2`; fit: `6`;
fill-and-crop: `10`). Fill preserves the aspect ratio by cropping,
whereas Stretch distorts to the destination rectangle.

WATER now has a shared renderer in `dlls/user32/desktop.c`:

- For an **explicit** DOS-Windows 3.x `water_win16_profile` (pre-9x,
  major 3), read the wallpaper *filename* and tile/center policy from
  WIN.INI and do not inherit the later registry picture/modes.
- For other host personalities, use the legacy tile setting as a
  fallback, then override from valid modern registry settings when
  present. Reject invalid/non-string registry values.
- The display path now renders Tile, Center, Stretch, Fit and Fill
  with 64-bit geometry intermediates for aspect-ratio calculations.
  It leaves the background pattern visible around center/fit, clips
  oversized centered images, and crops centered source regions for
  Fill rather than treating Fill as Stretch.

These changes are **rendering and configuration routing only**.
A Win3.x desktop process must actually receive the selected guest
profile; WATER does not yet boot genuine Windows 3.1 DOS/Win16
shell and driver stages. The modern `desk.cpl` display-settings
page is not evidence of an authentic Windows 3.1 Control Panel
Desktop applet or of a working wallpaper-position selection UI.

**Format boundaries:** normal wallpaper loading uses
`LoadImageW(... IMAGE_BITMAP ... LR_LOADFROMFILE)` with an OLE
picture fallback for other pictures. `BI_RLE4` / `BI_RLE8`
compressed BMPs are different from the uncompressed Win95/98
`LOGO.SYS` boot-logo BMP, and different again from VfW MSRLE32
video packets. Bitmap loading remains unverified for all indexed
RLE variants and early Win3.x original media.

A standalone C99 geometry harness reproduced the layout helper and
checked 11 deterministic center/stretch/fit/fill cases under
ASan/UBSan, including oversized, letterboxed, source-cropped and
offset-origin rectangles. **This was not a compiled WATER user32
module or guest wallpaper runtime test**. Full registry/INI
integration, actual 4/8-bit BMP palette behavior, display DPI,
and multi-monitor rendering remain unverified.

Sources:
- Microsoft KB Q83433 (Windows 3.1 Resource Kit WIN.INI desktop):
  https://www.infania.net/misc/kbarchive/kb/083/Q83433/index.html
- Microsoft theme-file wallpaper mappings (Win7+ Fit/Fill):
  https://learn.microsoft.com/en-us/windows/win32/controls/themesfileformat-overview
- Separate Windows wallpaper position API:
  https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/ne-shobjidl_core-desktop_wallpaper_position

## AOL / America Online installation evidence — scoped

AOL-related software really did change WIN.INI on **some** systems,
but do not generalize this to all AOL application versions.

The original cFos vendor configuration instructions describe
two AOL-associated ISDN support variants:
- **AOLISDN**: a `[windows] RUN=` entry such as
  `C:\\AOLISDN\\AOLISDND.EXE`; the accompanying SYSTEM.INI
  references include `VAOLISDN.VXD` and an `[AOLISDN]` section.
- **AOL-bundled OEMcFos**: a `[windows] run=` entry such as
  `C:\\WINDOWS\\OEMcFos\\cFosoemd.exe`; SYSTEM.INI may contain
  `device=vcFosoem.vxd` and `[cFosoem]`.

These are **vendor-documented removal targets**, not captured
before/after diffs from a particular AOL installer. The vendor says
'Recent AOL versions' for OEMcFos but does **not** identify an exact
AOL major/minor version in that passage. Consequently, mark the
precise AOL client version/edition, installation date, and whether
these components were installed by default as **unverified**.

AOL 3.0 Win16/Windows 3.1 and 32-bit Windows 95 installer branches
were already separated in the WHP audit; do not merge their setup
effects merely because both were marketed as '3.0'. A Trojan
masquerading as AOL components can also create RUN/LOAD entries:
the presence of an AOL path alone does not identify a legitimate
AOL installer. Recover original installer script/INF or a dated
before/after WIN.INI to resolve specific variants.

Sources:
- cFos original vendor troubleshooting and uninstall instructions:
  https://cfos.de/en/cfos-professional/app.htm
- Microsoft KB Q69844, load/run display semantics (Windows 3.0):
  https://jeffpar.github.io/kbarchive/kb/069/Q69844/
- Microsoft KB Q85476, Program Manager versus third-party shell:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/85476
- Microsoft KB Q81606, independent Startup group:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/81606
- Microsoft KB Q91204, Windows/DOS startup-group ordering:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/91204
- Microsoft KB Q83386, SYSTEM.INI shell= owner:
  https://jeffpar.github.io/kbarchive/kb/083/Q83386/
- F-Secure, Trojan.AOL.Buddy (negative control for attribution):
  https://www.f-secure.com/v-descs/aolbuddy

## Unresolved questions

1. Compare Win98 FE-specific GetProfile* return and truncation quirks.
2. Verify Win3 Program Manager load/run ordering and actual launches
   in a Windows 3.x guest; independently audit Win95/98 shell startup
   without unconditional Win3 Program Manager execution.
3. Preserve IniFileMapping distinction between Win9x/NT profiles.
4. Check profile comments, sections, code pages, double-NUL enumeration,
   small buffers, and cache invalidation.
5. Retain the prior Win16 [Compatibility] NE version threshold.
6. Recover exact original AOL installation builds or INF/script changes
   before assigning an ISDN RUN entry to a particular AOL version.
