/*
 * IFSMGR VxD implementation
 *
 * Copyright 1998 Marcus Meissner
 * Copyright 1998 Ulrich Weigand
 * Copyright 1998 Patrik Stridvall
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

/* NOTES
 *   These ioctls are used by 'MSNET32.DLL'.
 *
 *   I have been unable to uncover any documentation about the ioctls so
 *   the implementation of the cases IFS_IOCTL_21 and IFS_IOCTL_2F are
 *   based on reasonable guesses on information found in the Windows 95 DDK.
 */

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

/*
 * IFSMgr DeviceIO service
 */

#define IFS_IOCTL_21                100
#define IFS_IOCTL_2F                101
#define IFS_IOCTL_GET_RES           102
#define IFS_IOCTL_GET_NETPRO_NAME_A 103

/*
 * Windows 9x passes an x86 register packet here.  Keep the packet fixed-width
 * and independent of the architecture used to build Water.
 */
struct win32apireq
{
    DWORD ar_proid;
    DWORD ar_eax;
    DWORD ar_ebx;
    DWORD ar_ecx;
    DWORD ar_edx;
    DWORD ar_esi;
    DWORD ar_edi;
    DWORD ar_ebp;
    WORD  ar_error;
    WORD  ar_pad;
};

static void win32apireq_to_i386_context(const struct win32apireq *request,
                                        I386_CONTEXT *context)
{
    memset(context, 0, sizeof(*context));

    context->ContextFlags = CONTEXT_I386_INTEGER | CONTEXT_I386_CONTROL;
    context->Eax = request->ar_eax;
    context->Ebx = request->ar_ebx;
    context->Ecx = request->ar_ecx;
    context->Edx = request->ar_edx;
    context->Esi = request->ar_esi;
    context->Edi = request->ar_edi;

    /* The VxD packet only exposes part of the x86 control state. */
    context->Ebp = request->ar_ebp;
}

static void i386_context_to_win32apireq(const I386_CONTEXT *context,
                                        const struct win32apireq *request,
                                        struct win32apireq *reply)
{
    /*
     * proid/error/pad are packet metadata, not CPU registers. Preserve them
     * unless a service grows explicit handling for those fields.
     */
    *reply = *request;

    reply->ar_eax = context->Eax;
    reply->ar_ebx = context->Ebx;
    reply->ar_ecx = context->Ecx;
    reply->ar_edx = context->Edx;
    reply->ar_esi = context->Esi;
    reply->ar_edi = context->Edi;
    reply->ar_ebp = context->Ebp;
}

/*
 * The interrupt dispatcher consumes guest x86 register state.  This is not the
 * host exception CONTEXT on ARM/ARM64.
 */
extern void WINAPI __wine_call_int_handler16(BYTE intnum, I386_CONTEXT *context);

/***********************************************************************
 *           DeviceIoControl   (IFSMGR.VXD.@)
 */
BOOL WINAPI IFSMGR_DeviceIoControl(DWORD dwIoControlCode, LPVOID lpvInBuffer, DWORD cbInBuffer,
                                   LPVOID lpvOutBuffer, DWORD cbOutBuffer,
                                   LPDWORD lpcbBytesReturned,
                                   LPOVERLAPPED lpOverlapped)
{
    TRACE("(%u,%p,%u,%p,%u,%p,%p)\n",
          (unsigned int)dwIoControlCode, lpvInBuffer, (unsigned int)cbInBuffer,
          lpvOutBuffer, (unsigned int)cbOutBuffer, lpcbBytesReturned, lpOverlapped);

    if (lpcbBytesReturned) *lpcbBytesReturned = 0;

    switch (dwIoControlCode)
    {
    case IFS_IOCTL_21:
    case IFS_IOCTL_2F:
        {
            I386_CONTEXT context;
            const struct win32apireq *request = lpvInBuffer;
            struct win32apireq *reply = lpvOutBuffer;

            if (!request || cbInBuffer < sizeof(*request) ||
                !reply || cbOutBuffer < sizeof(*reply))
            {
                SetLastError(ERROR_INSUFFICIENT_BUFFER);
                return FALSE;
            }

            TRACE("Control '%s': "
                  "proid=0x%08x, eax=0x%08x, ebx=0x%08x, ecx=0x%08x, "
                  "edx=0x%08x, esi=0x%08x, edi=0x%08x, ebp=0x%08x, "
                  "error=0x%04x, pad=0x%04x\n",
                  (dwIoControlCode == IFS_IOCTL_21) ? "IFS_IOCTL_21" : "IFS_IOCTL_2F",
                  (unsigned int)request->ar_proid, (unsigned int)request->ar_eax,
                  (unsigned int)request->ar_ebx, (unsigned int)request->ar_ecx,
                  (unsigned int)request->ar_edx, (unsigned int)request->ar_esi,
                  (unsigned int)request->ar_edi, (unsigned int)request->ar_ebp,
                  request->ar_error, request->ar_pad);

            win32apireq_to_i386_context(request, &context);
            __wine_call_int_handler16(dwIoControlCode == IFS_IOCTL_21 ? 0x21 : 0x2f,
                                      &context);
            i386_context_to_win32apireq(&context, request, reply);

            if (lpcbBytesReturned) *lpcbBytesReturned = sizeof(*reply);
            return TRUE;
        }

    case IFS_IOCTL_GET_RES:
        FIXME("Control 'IFS_IOCTL_GET_RES' not implemented\n");
        return FALSE;

    case IFS_IOCTL_GET_NETPRO_NAME_A:
        FIXME("Control 'IFS_IOCTL_GET_NETPRO_NAME_A' not implemented\n");
        return FALSE;

    default:
        FIXME("Control %u not implemented\n", (unsigned int)dwIoControlCode);
        return FALSE;
    }
}
