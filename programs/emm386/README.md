# Water EMM386.EXE

This directory owns the DOS 386 expanded/upper-memory manager personality.

Recovered ownership so far:

- XMS function 10h: Request Upper Memory Block.
- XMS function 11h: Release Upper Memory Block.

Microsoft's documented MS-DOS memory architecture places these UMB-provider
requests in EMM386 when it is loaded. HIMEM.SYS owns the other XMS/HMA/A20
services; MS-DOS/NTDOS owns the later INT 21h memory-allocation policy after
DOS links the UMBs.

Still missing here:

- a real EMM386.EXE loadable DOS driver artifact;
- INT 67h EMS 4.0 services;
- EMS handles and 16-KiB logical pages;
- the 64-KiB EMS page frame and page mappings;
- RAM/NOEMS and I=/X=/FRAME= configuration;
- VCPI detection/services (INT 67h AX=DE00h family);
- NOVCPI behavior;
- XMS function 12h UMB reallocation.

KRNL386 currently compiles the UMB provider through a temporary compatibility
bridge. Do not move the implementation back into KRNL386 or HIMEM/XMS code.
