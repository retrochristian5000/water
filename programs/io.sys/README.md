# Water Windows 9x IO.SYS

This directory owns Windows 9x real-mode boot responsibilities that historically
run before WIN.COM and KRNL386.

The first recovered responsibility is MSDOS.SYS and CONFIG.SYS startup configuration parsing
(`config.c`). Microsoft documented Windows 95 IO.SYS as reading MSDOS.SYS,
processing CONFIG.SYS, selecting WinBootDir-dependent real-mode drivers, and
eventually invoking WIN.COM.

Water does **not** yet emit a bootable IO.SYS image from this directory. Do not
turn it into a normal PE/Win16 module just to obtain the filename: the eventual
artifact needs the proper real-mode boot-image ABI.

For the Windows 95/98 line, that ABI includes the MSLOAD boot prefix: the disk
bootstrap checks a leading MZ signature, but the MZ-shaped fields participate in
the IO.SYS boot protocol and must not be handed to Water's generic DOS/NE/PE
image loader. In particular, a future IO.SYS builder must preserve the loader's
real-mode load-size/header semantics rather than assuming that an MZ signature
means a conventional DOS EXE followed by an NE or PE image.

KRNL386 currently reaches this implementation through a temporary source bridge
only to preserve compatibility while the Win9x boot chain is being reconstructed.

Windows Me is a distinct Win9x boot personality. Its normal hard-disk IO.SYS
path bypasses the older real-mode CONFIG.SYS/AUTOEXEC driver startup and moves
toward VMM32 directly, while the protected-mode Win16 environment still retains
KRNL386.EXE. Water therefore keeps Me's KRNL386 compatibility surface separate
from its reduced real-mode boot policy.


The CONFIG.SYS parser was recovered from KRNL386's INT 21h implementation.
IO.SYS owns that parse; INT 21h now consumes the resulting BUFFERS, LASTDRIVE,
BREAK and UMB policy instead of opening CONFIG.SYS itself.

## MSDOS.SYS kernel versus text boot configuration

For MS-DOS 6.22 and DOS-hosted Windows 3.x/WfW, MSDOS.SYS is a
**binary DOS kernel** loaded through the earlier IO.SYS boot stage.
It is not an INI file, PE image, or Windows component. Windows 95/98
instead incorporate the DOS kernel into IO.SYS and use **text**
MSDOS.SYS for [Paths] and [Options]. Windows Me retains a text
configuration file but has a different reduced real-mode startup.
Windows 95 dual boot may preserve the earlier kernel as MSDOS.DOS.

The IOSYS_InitConfig compatibility bridge now selects text parsing only
for explicit Win95/98/Me guest identities. It also rejects binary or
unrecognizable files rather than treating the MSDOS.SYS filename as
proof of a text config. Both boot-drive and DoubleBuffer state reset
before an excluded DOS, Windows 3.x, or NT guest returns.

**Not implemented:** loading/executing the MS-DOS 6.22 MSDOS.SYS
kernel image, real-mode boot transfer, or authenticating a boot image.
DOS 6.x boot ownership must not be folded into the Win9x INI parser.
Regression: python3 -m unittest discover -s scripts/tests -p test_iosys_doublebuffer.py

Sources:
- https://techshelps.github.io/MSDN/DNWIN95/HTML/S628D.HTM
- https://techshelps.github.io/MSDN/KB/Source/zMiscellaneous/Q118579.htm

## Windows 95/98 startup LOGO.SYS BMP boundary

Windows 95/98 startup logos are **not** RLE4/RLE8 DIBs in the normal
GDI pipeline. The external `LOGO.SYS` file is a BMP with a 320 x 400
pixel raster, 8 bits per pixel (256 palette entries) and BI_RGB
(uncompressed) pixel data. The familiar startup-logo appearance uses
horizontal display scaling. The original animated color bar also depends
on palette cycling; successfully decoding the pixels alone does not
reproduce that effect. The nominal 40-byte BITMAPINFOHEADER + 14-byte
BMP file header + 1024-byte palette + 128000-byte raster totals 129078
bytes for that common header layout; validate the actual BMP offsets
rather than using only this total as a signature.

Boot-owner behavior, not yet executable in Water:
- Read `[Options] Logo=0|1` from the **boot-root** text `MSDOS.SYS`.
  `Logo=0` suppresses the startup logo; neither a generic GDI bitmap
  call nor the Win16 KERNEL loader should be substituted.
- When enabled, prefer a valid external `LOGO.SYS` from the boot root.
  The historical fallback is the default image embedded in `IO.SYS`;
  absence of a separate `LOGO.SYS` does **not** mean a missing logo.
  With DriveSpace, the boot root may be the **uncompressed host drive**,
  not the apparent Windows system drive.
- Validate BMP file header, declared pixel offset, DIB dimensions,
  8-bit BI_RGB layout, full 256-entry palette and raster bounds before
  committing to display. Preserve palette ordering and animation.
- `LOGOW.SYS` and `LOGOS.SYS` are separate shutdown screens associated
  with the Windows directory; they must not be merged into the IO.SYS
  startup-logo lookup. The OEM preinstallation `Oemstamp.exe` input
  logo is a separate 156 x 22 image workflow that generates `LOGO.SYS`.

The existing RLE fixes in `dlls/msrle32/msrle32.c` and
`dlls/win32u/dib.c` protect **other** compressed graphics but do not
implement this real-mode boot-logo path. Water currently does **not**
perform the filesystem lookup, validate the boot BMP, render VGA
graphics or animate its palette. Do not claim that a Win32 bitmap
decoder restores the Windows 95/98 startup screen.

Historical references:
- Windows 95 startup-logo documentation (320 x 400, 256 colors, Logo=0
  and color cycling):
  https://www2.isye.gatech.edu/~mgoetsch/cali/Windows%20Configuration/Windows%20Configuration.pdf
- Period Windows 95 FAQ documenting the boot-root override and IO.SYS
  embedded-image fallback:
  https://www.zisman.ca/FAQ/faq95.htm
- Microsoft KB Q216347, `Oemstamp.exe` and the distinct OEM logo flow:
  https://helparchive.huntertur.net/document/108106

## CONFIG.SYS SWITCHES= state

The IO.SYS parser also records `SWITCHES=` (case-insensitive) with the
classic `/F`, `/K`, `/N`, `/W` and Win98 `/E[:n]` options. `/E` alone
records an EBIOS relocation suppression request (0 bytes); `/E:n`
requires 48–1024 bytes and rounds up to a multiple of 16. Missing `/E`
is distinguishable with `-1`. Invalid or unknown directives have no
partial effect. Adjacent and whitespace-separated option tokens are
accepted. DOS 6.x help documents `/W` for a moved WINA20.386; Windows 98
CONFIG.TXT instead documents `/E` for the EBIOS behavior. Keep these
historical variants separate when implementing actual boot behavior.

These are **recorded startup settings**, not yet working boot overrides:
Water still uses a KRNL386 compatibility bridge and cannot currently
suppress an IO.SYS keyboard check, relocate EBIOS, or locate WINA20.386
at real-mode boot. Windows Me's existing skip of DOS boot directives remains
in effect; no Win9x option is silently projected into NT or Win3.x.

Parser regression: `python3 -m unittest discover -s scripts/tests -p test_iosys_switches.py`.

Sources: Microsoft Windows 98 CONFIG.TXT (KB Q232557; mirrored at
https://helparchive.huntertur.net/document/106583) and MS-DOS 6.22 HELP
(https://www.infania.net/misc/dos622help/switches.html).

## CONFIG.SYS ANSI.SYS request tracking (Windows 95/98)

The IO.SYS parser recognizes `DEVICE=ANSI.SYS` and
`DEVICEHIGH=[drive:][path]ANSI.SYS` as *requested* legacy character
drivers, including documented `/X` (extended-key remapping), `/K`
(84-key keyboard compatibility) and `/R` (readability-oriented
scrolling) switches. File basename matching is ASCII case-insensitive.
Unrelated device names and malformed options do not alter existing
boot state. The last valid ANSI.SYS directive supplies the saved
switches. CONFIG.SYS selected-section filtering still applies; the
Windows Me boot-personality skip remains intact.

`ansi_device_requested` deliberately does **not** mean that ANSI.SYS
was loaded, found on disk or attached to CON. Water does not yet
implement a real-mode character-driver loader, strategy/interrupt
entry points, CON replacement, INT 29h interception or a stateful
ANSI.SYS output/keyboard escape interpreter. No Win32-console VT
processing flag is implicitly enabled, and no host terminal escape
handling substitutes for this DOS driver.

Runtime implementation boundaries:
- IO.SYS resolves and loads the requested DOS driver from the boot
  environment; its load result, not parser acceptance, controls
  installation state and DEV/DEVICEHIGH placement.
- DOS CON output must feed a streaming escape interpreter **only**
  when a compatible driver is actually installed. Preserve parser
  state between separate INT 21h writes; leave redirected file bytes
  untouched and keep direct video-memory output a separate path.
- Apply the active DOS OEM codepage to printable character bytes,
  separately from ESC (1Bh), '[' (5Bh), ASCII digits and control
  syntax. DOS 7 codepage switching and DBCS require separate tests.
- Preserve cursor, erase, SGR color, saved position, mode and keyboard
  remapping behavior in historical phases, including DOS/Win9x
  differences and invalid-sequence recovery. ANSI.SYS is not a
  generic modern UTF-8/VT100 compatibility switch.

Parser regression is in existing
`scripts/tests/test_iosys_switches.py`; it compiles the actual
`IOSYS_ParseAnsiDevice` function as standalone C.

References: MS-DOS 6.22 HELP,
https://www.infania.net/misc/dos622help/ansi.sys.html ;
Microsoft Windows 98 MSDOSDRV.TXT (archived),
https://www.betaarchive.com/wiki/index.php/Windows_98/4.10.1546/msdosdrv.txt ;
MS-DOS v2 `ANSI.DOC`,
https://github.com/microsoft/MS-DOS/blob/main/v2.0/bin/ANSI.DOC .

## MSDOS.SYS [Options] DoubleBuffer (Windows 95/98)

Windows 95 RTM attests `DoubleBuffer=0|1|2` in the *text* MSDOS.SYS
`[Options]` section. This is **not** the binary MSDOS.SYS kernel of
earlier DOS, not DOS `BUFFERS=`, and not disk compression
`DBLSPACE.BIN` / `DRVSPACE.BIN`.

- `0`: no double-buffering requested by this MSDOS.SYS setting (the
  documented default).
- `1`: conditional double buffering for controllers needing it
  (commonly certain SCSI controllers).
- `2`: unconditional double buffering, even when not detected as necessary.

`IOSYS_ParseDoubleBuffer` accepts these exact numeric modes, ignoring
outer ASCII spaces/tabs; rejects invalid/ambiguous input atomically.
`IOSYS_InitConfig` reads `[Options] DoubleBuffer` from the already
selected boot-root MSDOS.SYS and resets the file-policy state on each
initialization. `IOSYS_GetMsdosDoubleBufferMode` exposes **only this
parsed file-mode**, not a statement that double buffering is active.

**Not yet implemented:** real boot of `IO.SYS`, automatic loading of
`DBLBUFF.SYS`, controller need detection, the independent
`HKLM\\System\\CurrentControlSet\\Control\\WinBoot\\DoubleBuffer`
registry control, or `CONFIG.SYS DEVICE=DBLBUFF.SYS /D+` handling.
Do not emulate one of those merely because `DoubleBuffer` parsed, and
do not propagate its state to Win3.x, NT/WOW, or Windows Me boot paths.

A Microsoft Windows 95 CD-ROM MSDOSDRV.TXT extract (dated August 1995)
documents DBLBUFF.SYS and `/D+`:
https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/135485

Microsoft KB Q118579 documents the values 0, 1 and 2:
https://techshelps.github.io/MSDN/KB/Source/zMiscellaneous/Q118579.htm

Microsoft KB Q174018 documents IO.SYS loading DBLBUFF.SYS during early boot
and describes the *separate* WinBoot registry setting:
https://techshelps.github.io/MSDN/KB/Source/win95x/q174018.htm

Standalone parser regression: `python3 -m unittest discover -s scripts/tests -p test_iosys_doublebuffer.py`.

## BIOS hard-disk MBR boot preparation (Windows 95 RTM)

The **classic** BIOS hard-disk chain is: BIOS reads disk LBA 0 (MBR)
-> MBR selects the active primary partition -> partition VBR checks its
FAT BIOS Parameter Block and loads `IO.SYS` -> Win95 `IO.SYS` processes
text `MSDOS.SYS` and DOS boot configuration -> `WIN.COM` starts Windows.
The MBR does not itself find `IO.SYS`, and a floppy boot or an OEM boot
manager may have a different first-stage path.

Water now contains **read-only, pure** MBR/VBR decoders in `config.c`:

- `IOSYS_ParseMbrBootPartition` requires a 512-byte supplied MBR
  sector, the known total disk size in 512-byte sectors, a `55 AA`
  signature, exactly one active partition, valid status flags, a
  non-empty partition type and an in-range nonzero start/length. It
  returns disk-absolute VBR LBA and count **only on full success**.
- `IOSYS_IsWin95RtmPartitionType` recognizes FAT12 `01`, FAT16
  `04`/`06`, and FAT16-LBA `0E`. The latter needs BIOS INT 13h
  extensions before a real boot can use it.
- `IOSYS_ValidateWin95RtmVbr` accepts the separately supplied
  active partition's VBR only when it has a valid boot signature,
  boot jump, 512-byte FAT12/16 BPB, self-consistent FAT capacity,
  partition-fit geometry and matching hidden-sector LBA. It rejects
  FAT32 (types `0B`/`0C`) because Win95 **RTM** predates OSR2.

**No real boot yet.** The decoders neither run MBR boot code nor read
physical disk LBA 0, select host devices, execute BIOS INT 13h, read
FAT directory chains, or transfer control into a bootable `IO.SYS`
image. Existing `iosys_read_boot_sector` reads a mapped **logical
volume's VBR**, not a physical drive's MBR. Its behavior is not
silently reinterpreted. Partition status and type are not a security
trust boundary against malicious boot-sector instructions; these
routines are for structure/compatibility validation only.

Regression: `python3 -m unittest discover -s scripts/tests -p test_iosys_mbr.py`.

Sources: Microsoft Windows 95 startup KB Q174018
(https://techshelps.github.io/MSDN/KB/Source/win95x/q174018.htm);
Microsoft partition type KB Q69912
(https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/69912);
PC/AT MBR layout
(https://wiki.osdev.org/MBR_%28x86%29).
