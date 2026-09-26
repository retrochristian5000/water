/*
 * PPPMAC VxD implementation
 *
 * Clean-room compatibility implementation for the Windows 95/98
 * virtual PPP driver.
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
#include "winternl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

#define PPPMAC_VERSION 0x0300

/***********************************************************************
 *           DeviceIoControl   (PPPMAC.VXD.@)
 *
 * Windows 95/98 PPPMAC exposes a DeviceIoControl entry point, but the
 * private control-code contracts are not publicly documented.  Keep
 * unknown requests explicit rather than guessing at PPP/NDIS state.
 */
BOOL WINAPI PPPMAC_DeviceIoControl( DWORD code, void *in_buffer, DWORD in_size,
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

/***********************************************************************
 *           VxDCall   (PPPMAC.VXD.@)
 */
DWORD WINAPI PPPMAC_VxDCall( DWORD service, I386_CONTEXT *context )
{
    (void)context;

    switch (LOWORD(service))
    {
    case 0x0000: /* PPP_Get_Version */
        TRACE("PPP_Get_Version -> 3.00\n");
        return PPPMAC_VERSION;

    default:
        FIXME("service %08lx not implemented\n", (unsigned long)service);
        return 0xffffffff;
    }
}
