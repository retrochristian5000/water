/*
 * Windows 95 INT13.VXD compatibility boundary (fixed disk interrupt VxD).
 *
 * The real-mode BIOS interrupt handler at programs/ntio.sys/int13.c
 * is a different subsystem. Do not translate BIOS disk 80h to a host DOS
 * volume here, invent IOS service contracts, or perform host disk I/O.
 * Water does not yet implement VMM32 static VxD initialization or INT 13h
 * guest interrupt interception.
 */
#include <stdarg.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

/***********************************************************************
 *           DeviceIoControl   (INT13.VXD.@)
 * There is no verified Windows 95 RTM user-mode IOCTL contract.
 */
BOOL WINAPI INT13_DeviceIoControl(DWORD code, void *input, DWORD input_size,
                                  void *output, DWORD output_size,
                                  DWORD *returned, OVERLAPPED *overlapped)
{
    TRACE("code %#lx, in %p/%lu, out %p/%lu, returned %p, overlapped %p\n",
          (unsigned long)code, input, (unsigned long)input_size,
          output, (unsigned long)output_size, returned, overlapped);

    if (returned) *returned = 0;
    SetLastError(ERROR_INVALID_FUNCTION);
    return FALSE;
}

/***********************************************************************
 *           VxDCall   (INT13.VXD.@)
 * VxD device ID 0020h does not establish any service ordinals or
 * CPU register semantics. Unknown calls must fail, without side effects.
 */
DWORD WINAPI INT13_VxDCall(DWORD service, I386_CONTEXT *context)
{
    FIXME("INT13.VXD service %#lx context %p unsupported\n",
          (unsigned long)service, context);
    return 0xffffffff;
}
