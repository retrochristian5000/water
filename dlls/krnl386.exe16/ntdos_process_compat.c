/*
 * Temporary KRNL386 -> NTDOS.SYS process compatibility bridge.
 *
 * NTDOS owns DOS termination semantics.  NT WOW delegates the final host task
 * exit to NTVDM DEM through WOW32.  DOS-based Windows keeps the old ExitThread
 * fallback until its DOSX/WIN386 process provider is separated.
 */

#include "windef.h"
#include "winbase.h"
#include "kernel16_private.h"
#include "../../programs/ntvdm/dem_process.h"

typedef BOOL (__cdecl *w32_dem_exit_proc)(WORD);

static void ntdos_compat_dem_exit(WORD retval)
{
    HMODULE wow32;
    w32_dem_exit_proc proc;

    if (!kernel_is_nt_wow_session())
    {
        ExitThread(retval);
        return;
    }

    if (!(wow32 = GetModuleHandleA("wow32.dll"))) return;
    if (!(proc = (w32_dem_exit_proc)GetProcAddress(wow32, "__wine_W32DemExitTask"))) return;
    proc(retval);
}

#define DEM_ExitTask ntdos_compat_dem_exit
#include "../../programs/ntdos.sys/process.c"
#undef DEM_ExitTask
