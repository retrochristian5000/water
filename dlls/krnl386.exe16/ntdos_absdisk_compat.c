/*
 * Temporary KRNL386 -> NTDOS.SYS absolute-disk compatibility bridge.
 *
 * NTDOS owns INT 25h/26h semantics. Host disk access is delegated to DEM.
 */
#include "../../programs/ntdos.sys/absdisk.c"
