/*
 * Temporary KRNL386 -> NTDOS.SYS compatibility bridge.
 *
 * INT 21h is DOS-kernel ownership. KRNL386 still links this source only
 * because Water does not yet execute the real guest NTDOS.SYS image.
 */
#include "../../programs/ntdos.sys/int21.c"
