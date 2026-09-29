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
