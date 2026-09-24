/*
 * VDEF VxD implementation
 *
 * Clean-room compatibility implementation for the Windows 95/98
 * default local file-system driver.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

/*
 * Windows 95/98 uses VDEF as the default local FSD after other local file
 * systems decline a volume.  Its ring-0 IFSMgr registration and FSD dispatch
 * contracts are not modeled here yet.  Keep this module deliberately small
 * rather than inventing mount or filesystem semantics.
 */

/***********************************************************************
 *           DeviceIoControl   (VDEF.VXD.@)
 */
BOOL WINAPI VDEF_DeviceIoControl( DWORD code, void *in_buffer, DWORD in_size,
                                  void *out_buffer, DWORD out_size,
                                  DWORD *bytes_returned, OVERLAPPED *overlapped )
{
    FIXME("code %lu, in %p/%lu, out %p/%lu, returned %p, overlapped %p: unsupported\n",
          (unsigned long)code, in_buffer, (unsigned long)in_size,
          out_buffer, (unsigned long)out_size, bytes_returned, overlapped);

    if (bytes_returned) *bytes_returned = 0;
    SetLastError( ERROR_NOT_SUPPORTED );
    return FALSE;
}
