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
