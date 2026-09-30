# NTIO.SYS ownership

This directory owns the MS-DOS BIOS layer of Water's Windows NT VDM model.

Microsoft's Windows NT 3.5 Resource Guide identifies `NTIO.SYS` as the
"MS-DOS emulation BIOS driver", while Microsoft's NT subsystem documentation
describes it as the VDM equivalent of DOS `IO.SYS`. That makes BIOS-facing
services a better owner match than KRNL386.EXE.

The first recovered slice contains BIOS INT 11h, 12h, 13h, 15h, 16h, 17h,
19h, and 1Ah implementations. The generic protected-mode interrupt routing,
Win16 task-vector state, and KERNEL relay entry points remain in KRNL386.

Water does not yet execute a standalone NTIO.SYS image. KRNL386 therefore
links these sources through temporary compatibility bridge translation units.
That bridge preserves DOS-based Windows profiles while ownership is separated.
It does **not** mean NTIO belongs to DOSX or WIN386. The follow-up boundary is:

- NT WOW: NTVDM loads/hosts NTIO BIOS services.
- Windows 3.x Standard mode: DOSX/real-mode BIOS provider.
- Windows 3.x Enhanced mode: WIN386/VMM BIOS provider.

Once those providers exist, the compatibility bridge can be removed and the
BIOS implementation objects will no longer be part of KRNL386 at link time.
