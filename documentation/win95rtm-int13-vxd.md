# Windows 95 INT13.VXD: disk-layer ownership audit

**Status: source-confirmed missing module and VxD routing, compatibility
placeholder added; no native VMM32/VxD boot, BIOS interrupt intercept,
guest disk model or authenticated RTM INT13 binary.**

## Historical separation

Microsoft Q174018 lists `*INT13` and `*IOS` as **separate required
Windows 95 VxDs** normally incorporated into VMM32. Q137335 identifies
INT13 as fixed-disk interrupt VxD device ID **0020h**, and IOS as
device ID **0010h**. This identifies VxD devices, **not function/service
numbers**; do not guess services or return version 3.10 simply because
a separately published diagnostic lists it.

Microsoft Q158194 (OSR2 OEM CD) inventories `INT13.VXD` at **9,931
bytes** (August 24, 1996). The Q192841 AMDK6UPD OSR2 update lists
`INT13.VXD` **4.00.1112, 9,955 bytes** (January 15, 1997). These
two OSR2 witnesses must not be mislabeled **RTM** and the hotfix
inclusion does not prove INT13 had the IOS timing divide bug.

## Exact Water owner map

| Surface | Water source | Boundary |
| --- | --- | --- |
| BIOS INT 13h register handlers, AH=00/01/02/03/08, etc | `programs/ntio.sys/int13.c` | DOS/NTVDM BIOS compatibility; linked into KRNL386 by temporary `ntio_int13_compat.c`; **not Win95 INT13.VXD** |
| Real-mode disk boot | `programs/io.sys/config.c` | Synthetic/read-only physical MBR/VBR FAT decoding; no BIOS boot image or INT13 disk reader |
| Protected-mode INT 13h virtualization | **`dlls/int13.vxd`** | Separate Win95 fixed-disk interrupt VxD slot; initial fail-closed compatibility endpoint only |
| Protected-mode I/O Supervisor | `dlls/ios.vxd/ios.c` | IOS device 0010h; `IOS_FIND_INT13_DRIVE` not yet implemented |
| Physical disk PDRs | None matching ESDI_506.PDR/SCSIPORT.PDR | Never conflate Windows 95 `.PDR` with NT `scsiport.sys` |

### Concrete coordinate/ownership flaw to avoid

Water's NTIO `int13_get_drive()` maps BIOS hard-disk DL=80h and
above to the Nth host `DRIVE_FIXED` **logical volume**, then
`int13_transfer()` uses `DOSVM_RawRead/RawWrite` on that volume.
That is at best a DOS VDM compatibility approximation: BIOS disk
80h is a **physical disk** whose MBR is normally sector zero.
Its C: volume may start at LBA 63 or 2048, and disks can contain
multiple partitions. Transplanting the NTIO implementation into
INT13.VXD could write to the wrong disk sectors; it is not a
safe interpretation of real BIOS disk coordinates. Physical disk
enumeration, drive order, read/write bounds, V86 state and
protected-mode IOS fallback remain missing.

### Changes

- `configure.ac` registers `dlls/int13.vxd`, containing a Makefile,
  module exports and isolated implementation.
- `dlls/krnl386.exe16/vxd.c` routes VxD ID 0020h to the new
  compatibility module (parallel to existing 0010h -> IOS).
- Both `DeviceIoControl` and `VxDCall` reject unsupported requests,
  with no host disk operations. **Those two exported compatibility
  entrypoints are not attested native INT13 RTM exports.** No
  pretend-success or guessed register values are introduced.
- Existing NTIO, IOS and IO.SYS code remains unchanged.

### Next verification

Acquire an authentic 4.00.950 INT13 component from legitimate
Windows 95 RTM media or VMM32, record file SHA256, DDB device ID,
control entry/register conventions and linkages to IOS/BIOSXLAT;
compare separately with OSR2. Synthetic source tests should confirm
the module, ID routing, and failure/zero-output guard; full Water
compilation and guest startup/interrupt tests are not yet completed.

Historical references:

- Q174018 startup VxD list: https://jeffpar.github.io/kbarchive/kb/174/Q174018/
- Q137335 fixed-disk VxD device ID: https://jeffpar.github.io/kbarchive/kb/137/Q137335/
- Q158194 OSR2 OEM CD: https://jeffpar.github.io/kbarchive/kb/158/Q158194/
- Q192841 AMD K6-2 update: https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/192841
- Q126855 large IDE/INT13 extensions: https://helparchive.huntertur.net/document/106867
