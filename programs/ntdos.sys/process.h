/*
 * NTDOS.SYS process termination compatibility.
 */
#ifndef __WATER_NTDOS_PROCESS_H
#define __WATER_NTDOS_PROCESS_H

#include "windef.h"
#include "winnt.h"

void NTDOS_Exit(WORD retval);
void WINAPI DOSVM_Int20Handler(I386_CONTEXT *context);

#endif /* __WATER_NTDOS_PROCESS_H */
