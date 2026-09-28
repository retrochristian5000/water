/*
 * Windows 9x IOS VxD compatibility layer
 *
 * Copyright 2026 Water contributors
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
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "winioctl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

enum ios_service
{
    IOS_GET_VERSION = 0x0000,
    IOS_REGISTER_DEVICE = 0x0001,
    IOS_FIND_INT13_DRIVE = 0x0002,
    IOS_GET_DEVICE_LIST = 0x0003,
    IOS_SEND_COMMAND = 0x0004,
    IOS_COMMAND_COMPLETE = 0x0005,
    IOS_SYNCHRONOUS_COMMAND = 0x0006,
    IOS_REGISTER = 0x0007,
    IOS_REQUESTOR_SERVICE = 0x0008,
    IOS_EXCLUSIVE_ACCESS = 0x0009,
    IOS_SEND_NEXT_COMMAND = 0x000a,
    IOS_SET_ASYNC_TIMEOUT = 0x000b,
    IOS_SIGNAL_SEMAPHORE_NO_SWITCH = 0x000c,
    IOS_IDLE_STATUS = 0x000d,
    IOS_MAP_IORS_TO_I24 = 0x000e,
    IOS_MAP_IORS_TO_I21 = 0x000f,
    IOS_PRINT_LOG = 0x0010,
};

static WORD ios_win_version(void)
{
    DWORD version = GetVersion();

    return (LOBYTE(version) << 8) | HIBYTE(version);
}

/***********************************************************************
 *           DeviceIoControl   (IOS.VXD.@)
 *
 * IOS is primarily a ring-0 VxD service provider. Keep an explicit
 * DeviceIoControl entry point so CreateFile("\\\\.\\IOS.VXD") can resolve
 * the module without pretending undocumented user-mode controls succeeded.
 */
BOOL WINAPI IOS_DeviceIoControl(DWORD code, LPVOID in_buffer, DWORD in_size,
        LPVOID out_buffer, DWORD out_size, LPDWORD returned, LPOVERLAPPED overlapped)
{
    TRACE("(%lu,%p,%lu,%p,%lu,%p,%p)\n", code, in_buffer, in_size,
            out_buffer, out_size, returned, overlapped);

    if (returned)
        *returned = 0;

    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

/***********************************************************************
 *           VxDCall   (IOS.VXD.@)
 *
 * VxD id 0010h is the Windows 9x BlockDev/IOS service table. Only
 * contracts that Water can currently satisfy without fabricating IOP,
 * DCB, VRP, or calldown state are implemented here.
 */
DWORD WINAPI IOS_VxDCall(DWORD service, I386_CONTEXT *context)
{
    WORD function = LOWORD(service);

    TRACE("service %04x, context %p\n", function, context);

    switch (function)
    {
    case IOS_GET_VERSION:
        return ios_win_version();

    case IOS_REGISTER_DEVICE:
    case IOS_FIND_INT13_DRIVE:
    case IOS_GET_DEVICE_LIST:
    case IOS_SEND_COMMAND:
    case IOS_COMMAND_COMPLETE:
    case IOS_SYNCHRONOUS_COMMAND:
    case IOS_REGISTER:
    case IOS_REQUESTOR_SERVICE:
    case IOS_EXCLUSIVE_ACCESS:
    case IOS_SEND_NEXT_COMMAND:
    case IOS_SET_ASYNC_TIMEOUT:
    case IOS_SIGNAL_SEMAPHORE_NO_SWITCH:
    case IOS_IDLE_STATUS:
    case IOS_MAP_IORS_TO_I24:
    case IOS_MAP_IORS_TO_I21:
    case IOS_PRINT_LOG:
        FIXME("IOS service %04x is not implemented yet\n", function);
        return 0xffffffff;

    default:
        FIXME("Unknown IOS service %04x\n", function);
        return 0xffffffff;
    }
}
