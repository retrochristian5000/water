/*
 * Temporary KRNL386 -> NTIO.SYS BIOS compatibility bridge.
 *
 * Source ownership is NTIO.SYS. KRNL386 links this only until DOSX/WIN386
 * have their own BIOS providers and NT WOW calls NTIO through its VDM owner.
 */
#include "../../programs/ntio.sys/bios.c"
