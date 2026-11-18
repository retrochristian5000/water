# DOS API coverage and ownership register

Status: **SOURCE-AUDITED BASELINE; NOT A COMPLETE DOS API OR VERIFIED REAL-MODE RUNTIME** (2026-11-18).

This is the cross-component entry point for Water's DOS API work. Detailed
implementation evidence remains with the owning component; do not duplicate
or replace the existing IO.SYS, NTDOS.SYS, NTIO.SYS, and Win95 VxD ledgers.
"Has a handler" is not equivalent to guest-compatible behavior. For every
entry, record the *historical contract*, *guest version*, *execution mode*,
*source owner*, *actual behavior*, *test evidence*, and *remaining gap*.

## Personality boundaries

| Guest environment | Kernel / interface expectations | Water boundary |
| --- | --- | --- |
| MS-DOS 6.22 (and DOS-hosted Windows 3.x) | Earlier binary MSDOS.SYS kernel, FAT12/16, real-mode DOS API | `programs/io.sys` can parse relevant settings; it does **not** execute a DOS 6.x binary kernel |
| Windows 95 RTM (DOS 7.00) | Win9x IO.SYS with embedded DOS kernel; no FAT32 | `programs/io.sys` boot research; `programs/ntdos.sys/int21.c` has compatibility handlers, not a verified Win95 kernel |
| Windows 95 OSR2 / Windows 98 (DOS 7.10) | Win9x boot and FAT32 73xx services where applicable | Guest-version-gated FAT32 paths in `int21.c` and BPB readers; raw FAT32 runtime incomplete |
| Windows Me (DOS 8.00) | Distinct reduced real-mode boot policy | Do not inherit Win95 CONFIG.SYS boot loading automatically |
| Windows NT NTVDM | Virtual DOS services through NTDOS/NTIO/DEM | `programs/ntdos.sys`, `programs/ntio.sys`, DEM; not physical DOS or Win9x boot |
| Win16 / Win9x protected mode | KRNL386, WIN386, VMM/VxD, and PDR interfaces | Treat each as its own ABI; do not substitute NT/WDM or Win32 services |

**Release gate:** FAT32 availability follows the actual guest identity, not
the host filesystem, a spoofed DOS version, or Win16 GetVersion. Long
filename functionality must likewise be checked for the particular Windows
9x environment and execution mode before treating the entire 71xx range as
available. DOS APIs, BIOS calls, PCI configuration, Win9x VxD services and
NT kernel calls are **not** interchangeable namespaces.

## Source-audited API register (initial high-value slice)

"Compatibility path" means source is present but historical behavior and
guest execution have **not** been established. "Missing" means the
specific audited source has an explicit stub or no suitable path; it is
not proof that no unrelated source contains a similar function.

| Interface | Owning source / actual state | Priority and missing evidence |
| --- | --- | --- |
| `INT 21h, AH=0Ah` buffered input | `programs/ntdos.sys/int21.c`: explicitly not supported | **High:** buffer length/count/CR, redirection, Ctrl-C, DOS 6.22 vs 7.x guest tests |
| `INT 21h, AH=0Ch` flush/read | `int21.c`: dispatch exists; input flush is incomplete and 0Ah remains unsupported | **High:** flush and chained-input regression |
| `INT 21h, AH=31h` TSR | `int21.c`: explicit terminate-and-stay-resident stub | **High:** PSP/MCB lifetime, interrupt chaining, InDOS-safe reentrancy |
| `INT 21h, AH=34h` InDOS address | `int21.c`: compatibility flag/address exists | **High:** segment layout, nested calls, critical-error ordering and actual resident behavior |
| `INT 21h, AH=48h/49h/4Ah` DOS memory | `int21.c`: allocation/free compatibility paths; resize 4Ah explicitly unsupported | **High:** paragraph sizing, MCB ownership, block failure/resize contracts |
| `INT 21h, AX=71xx` long names | `int21.c`: some LFN dispatch exists | **High:** audit every subfunction, version gate, errors, codepage and redirected drives |
| `INT 21h, AX=7302h` FAT32 extended DPB | `int21.c`: version-gated compatibility path, using IO.SYS BPB when available | **High:** compare binary structure to an authentic DOS 7.10 image |
| `INT 21h, AX=7303h` FAT32 free space | `int21.c`: version-gated host-backed calculation; synthesizes some totals | **High:** verify DOS 7.10 sector/cluster values, overflow and FAT32 guest semantics |
| `INT 2Fh, AX=4800h/4810h` DOSKEY | `dlls/krnl386.exe16/int2f.c`: absent installation probe; resident-service call not implemented | **Medium:** do not invent DOSKEY installation, macro buffer or TSR chain |
| `INT 25h/26h` absolute disk | `programs/ntdos.sys/absdisk.c`: compatibility handlers forward to DEM | **High safety:** validate guest volume identity, bounds, extended packet and write isolation |
| `INT 13h` BIOS disks | `programs/ntio.sys/int13.c`: VDM BIOS approximation; separate `dlls/int13.vxd` Win95 slot | **High safety:** BIOS physical-disk identity must not be confused with a host C: volume |
| `INT 1Ah, AX=B101h` PCI BIOS probe | `programs/ntio.sys/bios.c`: no PCI B1xx handler in the audited INT 1Ah dispatcher | **Medium:** check BIOS service presence and version before PCI configuration calls; do not infer native PCIe support |
| DOS installable `.SYS` strategy/interrupt request packets | `programs/io.sys`: config parser can record driver requests but no real-mode character-driver loader/ABI | **High for drivers:** verify packet structure, driver header, initialization and device I/O, independently from VxD or NT `.SYS` |

An `INT 1Ah` PCI BIOS service being unavailable is **not** a diagnosis of
a missing PCIe host controller driver. PCI BIOS enumeration, PCI/PCIe
configuration access, device-specific bus mastering, DMA, and each guest OS's
driver model require independent checks. Also, PCI Express extended
configuration is not established by a successful legacy PCI BIOS probe.

## Work order and validation rules

1. Preserve and index the existing source: `programs/ntdos.sys/README.md`,
   `programs/io.sys/README.md`, `programs/ntio.sys/README.md`,
   `documentation/fat32-win9x.md`, and
   `documentation/win95rtm-int13-vxd.md`.
2. Test DOS 6.22, DOS 7.00, DOS 7.10, Me and NTVDM separately. Every
   capability gets explicit supported, unsupported and unknown states.
3. For an individual service, record register inputs/outputs, CF/AX error
   behavior, pointer representation (16:16 vs selectors), buffer sizes,
   structure packing, reentrancy and side effects **before** implementing.
4. Implement the smallest missing contract in its owning component. Do not
   transplant a VDM shim into physical IO.SYS, use host filesystem geometry
   as proof of a guest FAT volume, or make raw disk writes to test discovery.
5. Track four distinct results: **source audited**, **compiled**,
   **synthetic regression passed**, **authentic guest runtime confirmed**.
   Keep negative probes and unsupported returns rather than fake success.

Existing focused regression entry points (listed, **not run by this
documentation change**): `scripts/tests/test_iosys_fat32.py`,
`scripts/tests/test_int13_vxd_boundary.py`,
`scripts/tests/test_guest_sector_write_safety.py`, and
`scripts/tests/test_iosys_switches.py`. These do not demonstrate a
bootable DOS kernel or functional hardware driver.

## Historical references

- Microsoft MS-DOS 2.0 device-driver interface (historical architecture,
  not proof of later ABI identity):
  https://github.com/microsoft/MS-DOS/blob/main/v2.0/bin/DEVDRIV.DOC
- Ralf Brown's Interrupt List (reference for per-call historical
  descriptions; confirm release-specific behavior independently):
  https://fd.lod.bz/rbil/
- PCI BIOS installation check, `INT 1Ah, AX=B101h`:
  https://fd.lod.bz/rbil/interrup/bios_expansion/1ab101.html
- Existing Water release-specific FAT32 evidence and limitations:
  [Windows 9x FAT32 ledger](fat32-win9x.md)
