/*
 * NTDOS.SYS process termination compatibility.
 *
 * NTDOS owns DOS termination semantics. The current Water implementation has
 * not recovered the full PSP/vector/handle teardown path yet, so the final
 * host task exit is delegated to DEM.
 */

#include "windef.h"
#include "winnt.h"

#include "process.h"
#include "../ntvdm/dem_process.h"

void NTDOS_Exit(WORD retval)
{
    DEM_ExitTask(retval);
}

void WINAPI DOSVM_Int20Handler(I386_CONTEXT *context)
{
    (void)context;
    NTDOS_Exit(0);
}
