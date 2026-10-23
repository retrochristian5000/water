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
