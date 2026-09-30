/*
 * Temporary KRNL386 -> NTDOS.SYS absolute-disk compatibility bridge.
 *
 * NTDOS owns INT 25h/26h semantics.  Host raw-disk access stays in NTVDM DEM
 * and crosses the process-local WOW32 boundary instead of being linked into
 * KRNL386.
 */

#include "windef.h"
#include "winbase.h"
#include "kernel16_private.h"
#include "../../programs/ntvdm/dem_disk.h"

typedef BOOL (__cdecl *w32_dem_absread_proc)(BYTE, DWORD, DWORD, BYTE *, BOOL);
typedef BOOL (__cdecl *w32_dem_abswrite_proc)(BYTE, DWORD, DWORD, const BYTE *, BOOL);

static HMODULE ntdos_get_wow32(void)
{
    if (!kernel_is_nt_wow_session()) return NULL;
    return GetModuleHandleA("wow32.dll");
}

static BOOL ntdos_compat_dem_read(BYTE drive, DWORD begin, DWORD nr_sect,
                                  BYTE *dataptr, BOOL fake_success)
{
    HMODULE wow32 = ntdos_get_wow32();
    w32_dem_absread_proc proc;

    if (!wow32) return FALSE;
    proc = (w32_dem_absread_proc)GetProcAddress(wow32, "__wine_W32DemAbsoluteRead");
    return proc ? proc(drive, begin, nr_sect, dataptr, fake_success) : FALSE;
}

static BOOL ntdos_compat_dem_write(BYTE drive, DWORD begin, DWORD nr_sect,
                                   const BYTE *dataptr, BOOL fake_success)
{
    HMODULE wow32 = ntdos_get_wow32();
    w32_dem_abswrite_proc proc;

    if (!wow32) return FALSE;
    proc = (w32_dem_abswrite_proc)GetProcAddress(wow32, "__wine_W32DemAbsoluteWrite");
    return proc ? proc(drive, begin, nr_sect, dataptr, fake_success) : FALSE;
}

#define DEM_AbsoluteRead ntdos_compat_dem_read
#define DEM_AbsoluteWrite ntdos_compat_dem_write
#include "../../programs/ntdos.sys/absdisk.c"
#undef DEM_AbsoluteRead
#undef DEM_AbsoluteWrite
