/*
 * Temporary in-process model of the Windows NT DOS kernel personality.
 *
 * Windows NT boots a 16-bit NTDOS.SYS inside the VDM and lets that kernel
 * cross into the 32-bit DOS emulation manager for host services.  Water does
 * not execute that boot chain in-process yet, so keep DOS-kernel semantics
 * behind this boundary instead of letting the process/image loader own them.
 */
#ifndef __WATER_NTVDM_DOSKRNL_H
#define __WATER_NTVDM_DOSKRNL_H

#include "dosvm.h"

enum dos_interrupt_result dos_kernel_handle_interrupt(struct dos_process *process,
                                                       BYTE vector);

#endif /* __WATER_NTVDM_DOSKRNL_H */
