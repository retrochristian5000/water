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
