# Water NTDOS.SYS

This directory owns the DOS-kernel side of NTVDM.

The NT5 build explicitly produces `NTDOS.SYS` from DOS-kernel objects such as
`msdisp`, `mscode`, `alloc`, `disk`, `file`, `handle`, `ioctl`,
`getset`, `path`, `search`, and `msproc`. Water should follow that
ownership instead of accumulating those services in KRNL386.EXE.

Recovered ownership so far:

- `int21.c`: DOS INT 21h dispatcher and DOS-kernel-visible data/services.
- `absdisk.c`: INT 25h/26h DOS semantics. Host I/O is delegated to DEM.
- `process.c`: INT 20h / DOS termination ownership. Host task exit is DEM.

Still to split/recover:

- conventional MCB allocation and resize logic from KRNL386 `dosmem.c`;
- PSP/PDB/process/EXEC state and SETVER handling;
- DOS files, SFT/JFT handles, CDS/current-directory state and redirector calls;
- proper DEM/BOP calls instead of direct Win32 host calls inside the current
  compatibility implementation;
- DOS dispatcher stacks, InDOS/critical-error state and INT 27h behavior;
- the actual flat 16-bit NTDOS.SYS build artifact and NTIO -> DEMLOADDOS boot.

KRNL386 compatibility bridges are temporary. They keep current WOW behavior
working while source ownership is corrected; they should disappear when the
guest NTDOS.SYS path is executable.

## DOSKEY.COM TSR compatibility: evidence and missing DOS services (2026-10-19)

DOSKEY is a historical **DOS** resident command-line editor, not an NT
`DOSKEY.EXE`/CMD.EXE facility or a hardware-driver binary. Windows 95
replaced older MS-DOS utilities including `DOSKEY.COM` during Setup
(Microsoft KB Q129581); this does not establish byte-identical behavior
between MS-DOS 5/6, Win95 RTM (MS-DOS 7.0) and later Win9x releases.
Neither an original Win95 DOSKEY executable nor a live guest trace has
been inspected for this audit.

### Evidence-backed interrupt contracts

| Interface | Historical contract | Current Water state |
| --- | --- | --- |
| `INT 2Fh, AX=4800h` | DOSKEY resident installation probe; `AL=0` absent, nonzero present. DOS 5/6's `AX=AA02h` is version-specific; `ES` identifies resident segment only when actually installed. | `dlls/krnl386.exe16/int2f.c` reports **absent** in its builtin protected-mode compatibility dispatcher; it installs no DOSKEY and claims no resident segment. |
| `INT 2Fh, AX=4810h` | `DS:DX` points to a 128-byte line buffer for original MS-DOS 5 DOSKEY; byte 0 is `80h`, byte 1 is returned character count excluding CR, text begins at byte 2 and includes the CR. `AX=0000h` success; a macro can require a second call even after successful empty output. | Not implemented; no history, macro engine, guest editor, or guest buffer writes. Other `48xxh` calls remain unimplemented. |
| `INT 21h, AH=0Ah` | General buffered standard input at `DS:DX`, with length, count and terminating CR; DOS 2+ permits redirected input, and zero maximum returns immediately. | `programs/ntdos.sys/int21.c` explicitly says `BUFFERED INPUT - not supported`. |
| `INT 21h, AH=0Ch` | Flush input, then dispatch the selected character/buffered input function, including `AH=0Ah`. | Dispatch exists, but flush is a FIXME and its `0Ah` target is a stub. |
| `INT 21h, AH=31h` | DOS 2+ terminate-and-stay-resident: `AL` exit code, `DX` paragraphs to retain (16 bytes per paragraph), no return. Requires PSP ownership and memory-lifetime handling. | Explicit TSR stub; no persistent resident process or service handler. |
| `INT 21h, AH=25h/35h` | Set/get interrupt vectors. | Water maps protected-mode vectors, not an independently verified real-mode TSR interrupt-chain ABI. |
| `INT 21h, AH=48h/49h/4Ah` | Conventional-memory allocate/free/resize. | Allocation and free have compatibility paths; resize is explicitly unsupported. |

Microsoft KB Q79245 reports that DOSKEY uses BIOS keyboard `INT 16h`
to observe interactive command input, but does not expand macros in
batch-file execution. Preserve that behavior boundary: reading lines
from redirected standard input, typing into COMMAND.COM, and delivering
a hardware keyboard interrupt are not interchangeable operations.
The MS-DOS 5 DOSKEY `80h` length rule must not silently become the
Windows 95 RTM `/LINE` contract without RTM documentation or binary
inspection.

### Ownership and implementation gates

- Keep **real-mode startup and CONFIG.SYS/AUTOEXEC policy** under
  `programs/io.sys`; DOSKEY is an optional loaded utility, not the boot
  kernel and not a `CONFIG.SYS DEVICE=` hardware driver.
- Keep **DOS buffered input, PSP/MCB residency, interrupt-vector state,
  InDOS/critical-error reentrancy and multiplex dispatch** on their
  respective DOS-kernel/VDM boundaries. The KRNL386 built-in `4800h`
  negative probe is a compatibility fallback, not an emulated TSR.
  Do not turn it into `AA02h`, a fabricated `ES` pointer, or an
  unconditional `4810h` success. A guest-installed interrupt handler
  needs its own verified hook and chaining semantics.
- Do not promote `NTDOS.SYS` (NTVDM), `KRNL386.EXE` (Win16 compatibility),
  or NT `DOSKEY.EXE` into the original Windows 95 IO.SYS/COMMAND.COM
  real-mode execution path merely because an interrupt number matches.
- Before a runtime implementation, capture Windows 95 RTM 4.00.950
  `DOSKEY.COM` version/hash and `DOSKEY /?` behavior from a disposable
  guest; compare with MS-DOS 5/6 separately. Test absent/present multiplex
  probes, buffer size and CR boundaries, empty and repeated macro reads,
  redirected input, vector chaining, TSR memory lifetime, and clean
  reentrancy. Never test a guest TSR against host interrupt vectors.
  Missing evidence must remain a failing or unimplemented status.

Source trail:
- Microsoft KB Q129581, Windows 95 replacement of older `DOSKEY.COM`:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/129581
- Microsoft KB Q79245, DOSKEY's `INT 16h` and interactive/batch distinction:
  https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/79245
- MS-DOS 6.22 Help (period command switches and residence):
  https://www.infania.net/misc/dos622help/doskey.html
- Ralf Brown's Interrupt List, DOSKEY multiplex:
  https://fd.lod.bz/rbil/interrup/keyboard/2f4800.html
  https://fd.lod.bz/rbil/interrup/keyboard/2f4810.html
- Ralf Brown's Interrupt List, DOS input and TSR:
  https://fd.lod.bz/rbil/interrup/dos_kernel/210a.html
  https://fd.lod.bz/rbil/interrup/dos_kernel/2131.html
