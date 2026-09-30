/*
 * NTVDM DOS-emulation-manager raw-disk host services.
 *
 * NTDOS.SYS owns INT 25h/26h semantics. The NT5 implementation crosses to
 * 32-bit DEM services for absolute disk I/O; Water keeps that host operation
 * here rather than inside KRNL386 or the guest DOS kernel.
 */

#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

#include "dem_disk.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntvdm);

BOOL DEM_AbsoluteRead(BYTE drive, DWORD begin, DWORD nr_sect, BYTE *dataptr,
                      BOOL fake_success)
{
    WCHAR root[] = {'\\','\\','.','\\','A',':',0};
    HANDLE h;

    TRACE("absolute disk read, drive %u, sector %lu, count %lu, buffer %p\n",
          drive, begin, nr_sect, dataptr);

    root[4] += drive;
    h = CreateFileW(root, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD read, size = nr_sect * 512;
        LARGE_INTEGER offset;
        BOOL ret;

        offset.QuadPart = (ULONGLONG)begin * 512;
        ret = SetFilePointerEx(h, offset, NULL, FILE_BEGIN) &&
              ReadFile(h, dataptr, size, &read, NULL) && read == size;
        CloseHandle(h);
        if (ret) return TRUE;
    }

    memset(dataptr, 0, nr_sect * 512);
    if (!fake_success) return FALSE;

    if (begin == 0 && nr_sect > 1) dataptr[512] = 0xf8;
    if (begin == 1) dataptr[0] = 0xf8;
    return TRUE;
}

BOOL DEM_AbsoluteWrite(BYTE drive, DWORD begin, DWORD nr_sect,
                       const BYTE *dataptr, BOOL fake_success)
{
    WCHAR root[] = {'\\','\\','.','\\','A',':',0};
    HANDLE h;

    TRACE("absolute disk write, drive %u, sector %lu, count %lu, buffer %p\n",
          drive, begin, nr_sect, dataptr);

    root[4] += drive;
    h = CreateFileW(root, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                    0, NULL);
    if (h != INVALID_HANDLE_VALUE)
    {
        DWORD written, size = nr_sect * 512;
        LARGE_INTEGER offset;
        BOOL ret;

        offset.QuadPart = (ULONGLONG)begin * 512;
        ret = SetFilePointerEx(h, offset, NULL, FILE_BEGIN) &&
              WriteFile(h, dataptr, size, &written, NULL) && written == size;
        CloseHandle(h);
        if (ret) return TRUE;
    }

    return fake_success;
}
