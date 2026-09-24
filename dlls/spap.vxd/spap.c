/*
 * SPAP VxD implementation
 *
 * Clean-room compatibility implementation for the Windows 95/98
 * Shiva Password Authentication Protocol control-protocol module.
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
 * SPAP is PPP protocol c027h.  Unlike PPPMAC, historical Windows VxD
 * listings do not assign SPAP a public VxD device id; PPPMAC loads it as
 * an installable control protocol through the PPP\CPList configuration.
 *
 * The private PPPMAC<->SPAP registration ABI is not publicly documented.
 * Do not manufacture authentication state or credentials until that ABI
 * is recovered.
 */

/***********************************************************************
 *           DeviceIoControl   (SPAP.VXD.@)
 */
BOOL WINAPI SPAP_DeviceIoControl( DWORD code, void *in_buffer, DWORD in_size,
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
