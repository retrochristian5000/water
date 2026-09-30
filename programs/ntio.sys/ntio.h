/*
 * Water NTIO.SYS BIOS service ownership.
 *
 * NTIO is the NT VDM BIOS owner. These declarations are temporarily callable
 * from KRNL386 while the DOSX/WIN386 BIOS provider split is completed.
 */
#ifndef __WATER_NTIO_SYS_H
#define __WATER_NTIO_SYS_H

#include "windef.h"
#include "winnt.h"

void WINAPI DOSVM_Int11Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int12Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int13Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int15Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int16Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int17Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int19Handler(I386_CONTEXT *);
void WINAPI DOSVM_Int1aHandler(I386_CONTEXT *);

#endif /* __WATER_NTIO_SYS_H */
