/*
 * Temporary KRNL386 -> NTDOS.SYS absolute-disk compatibility bridge.
 *
 * NTDOS owns INT 25h/26h semantics.  NT WOW crosses to NTVDM DEM for host
 * raw-disk access.  DOS-based Windows keeps the old host fallback here until
 * its DOSX/WIN386 provider path is split out; never use that fallback for an
 * NT WOW session.
 */

#include <string.h>

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

static BOOL krnl386_legacy_absolute_read(BYTE drive, DWORD begin, DWORD nr_sect,
                                         BYTE *dataptr, BOOL fake_success)
{
    WCHAR root[] = {'\\','\\','.','\\','A',':',0};
    HANDLE h;

    root[4] += drive;
    h = CreateFileW(root, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD read;

        SetFilePointer(h, begin * 512, NULL, FILE_BEGIN);
        ReadFile(h, dataptr, nr_sect * 512, &read, NULL);
        CloseHandle(h);
    }

    memset(dataptr, 0, nr_sect * 512);
    if (!fake_success) return FALSE;

    if (begin == 0 && nr_sect > 1) dataptr[512] = 0xf8;
    if (begin == 1) dataptr[0] = 0xf8;
    return TRUE;
}

static BOOL krnl386_legacy_absolute_write(BYTE drive, DWORD begin, DWORD nr_sect,
                                          const BYTE *dataptr, BOOL fake_success)
{
    WCHAR root[] = {'\\','\\','.','\\','A',':',0};
    HANDLE h;

    root[4] += drive;
    h = CreateFileW(root, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                    0, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written;

        SetFilePointer(h, begin * 512, NULL, FILE_BEGIN);
        WriteFile(h, dataptr, nr_sect * 512, &written, NULL);
        CloseHandle(h);
    }

    return fake_success;
}

static BOOL ntdos_compat_dem_read(BYTE drive, DWORD begin, DWORD nr_sect,
                                  BYTE *dataptr, BOOL fake_success)
{
    HMODULE wow32;
    w32_dem_absread_proc proc;

    if (!kernel_is_nt_wow_session())
        return krnl386_legacy_absolute_read(drive, begin, nr_sect, dataptr, fake_success);

    wow32 = ntdos_get_wow32();
    if (!wow32) return FALSE;
    proc = (w32_dem_absread_proc)GetProcAddress(wow32, "__wine_W32DemAbsoluteRead");
    return proc ? proc(drive, begin, nr_sect, dataptr, fake_success) : FALSE;
}

static BOOL ntdos_compat_dem_write(BYTE drive, DWORD begin, DWORD nr_sect,
                                   const BYTE *dataptr, BOOL fake_success)
{
    HMODULE wow32;
    w32_dem_abswrite_proc proc;

    if (!kernel_is_nt_wow_session())
        return krnl386_legacy_absolute_write(drive, begin, nr_sect, dataptr, fake_success);

    wow32 = ntdos_get_wow32();
    if (!wow32) return FALSE;
    proc = (w32_dem_abswrite_proc)GetProcAddress(wow32, "__wine_W32DemAbsoluteWrite");
    return proc ? proc(drive, begin, nr_sect, dataptr, fake_success) : FALSE;
}

#define DEM_AbsoluteRead ntdos_compat_dem_read
#define DEM_AbsoluteWrite ntdos_compat_dem_write
#include "../../programs/ntdos.sys/absdisk.c"
#undef DEM_AbsoluteRead
#undef DEM_AbsoluteWrite
