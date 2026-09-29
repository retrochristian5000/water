/*
 * NTDOS.SYS absolute-disk interrupt compatibility.
 *
 * Microsoft NT5 NTDOS keeps INT 25h/26h in MSCODE.ASM and crosses the
 * guest/host boundary through SVC_DEMABSDRD/SVC_DEMABSDWRT. Keep the DOS
 * register/packet semantics here and the host raw-disk operations in DEM.
 */

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"

#include "../../dlls/krnl386.exe16/dosexe.h"
#include "../ntvdm/dem_disk.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

void WINAPI DOSVM_Int25Handler(I386_CONTEXT *context)
{
    WCHAR drivespec[] = {'A', ':', '\\', 0};
    BYTE *dataptr = ldt_get_ptr(context->SegDs, context->Ebx);
    DWORD begin, length;

    drivespec[0] += AL_reg(context);
    if (GetDriveTypeW(drivespec) == DRIVE_NO_ROOT_DIR ||
        GetDriveTypeW(drivespec) == DRIVE_UNKNOWN)
    {
        SET_CFLAG(context);
        SET_AX(context, 0x0201);
        return;
    }

    if (CX_reg(context) == 0xffff)
    {
        begin   = *(DWORD *)dataptr;
        length  = *(WORD *)(dataptr + 4);
        dataptr = ldt_get_ptr(*(WORD *)(dataptr + 8), *(DWORD *)(dataptr + 6));
    }
    else
    {
        begin  = DX_reg(context);
        length = CX_reg(context);
    }

    if (!DEM_AbsoluteRead(AL_reg(context), begin, length, dataptr, TRUE))
    {
        SET_CFLAG(context);
        SET_AX(context, 0x0201);
        return;
    }
    RESET_CFLAG(context);
}

void WINAPI DOSVM_Int26Handler(I386_CONTEXT *context)
{
    WCHAR drivespec[] = {'A', ':', '\\', 0};
    BYTE *dataptr = ldt_get_ptr(context->SegDs, context->Ebx);
    DWORD begin, length;

    drivespec[0] += AL_reg(context);
    if (GetDriveTypeW(drivespec) == DRIVE_NO_ROOT_DIR ||
        GetDriveTypeW(drivespec) == DRIVE_UNKNOWN)
    {
        SET_CFLAG(context);
        SET_AX(context, 0x0201);
        return;
    }

    if (CX_reg(context) == 0xffff)
    {
        begin   = *(DWORD *)dataptr;
        length  = *(WORD *)(dataptr + 4);
        dataptr = ldt_get_ptr(*(WORD *)(dataptr + 8), *(DWORD *)(dataptr + 6));
    }
    else
    {
        begin  = DX_reg(context);
        length = CX_reg(context);
    }

    if (!DEM_AbsoluteWrite(AL_reg(context), begin, length, dataptr, TRUE))
    {
        SET_CFLAG(context);
        SET_AX(context, 0x0201);
        return;
    }
    RESET_CFLAG(context);
}
