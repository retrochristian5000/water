/*
 * NTVDM DOS-emulation-manager process host services.
 */

#include "windef.h"
#include "winbase.h"

#include "dem_process.h"

void DEM_ExitTask(WORD retval)
{
    ExitThread(retval);
}
