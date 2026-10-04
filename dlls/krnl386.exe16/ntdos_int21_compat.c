#include "win386.h"

static BYTE ntdos_compat_get_dos_oem_number(void)
{
    struct win386_session_info session;

    if (WIN386_QuerySession( &session ) &&
        session.dos_family == WATER_WIN386_DOS_FAMILY_PCDOS)
        return 0x00; /* IBM PC-DOS */

    return 0xff; /* Microsoft/NT DOS compatibility default */
}

#define NTDOS_GET_DOS_OEM_NUMBER() ntdos_compat_get_dos_oem_number()

/*
 * Temporary KRNL386 -> NTDOS.SYS compatibility bridge.
 *
 * INT 21h is DOS-kernel ownership. KRNL386 still links this source only
 * because Water does not yet execute the real guest NTDOS.SYS image.
 */
#define DEM_AbsoluteRead DOSVM_RawRead
#define DEM_AbsoluteWrite DOSVM_RawWrite
#include "../../programs/ntdos.sys/int21.c"
#undef DEM_AbsoluteRead
#undef DEM_AbsoluteWrite
#undef NTDOS_GET_DOS_OEM_NUMBER
