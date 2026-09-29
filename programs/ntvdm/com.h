/*
 * DOS .COM image loader for Water NTVDM.
 */
#ifndef __WATER_NTVDM_COM_H
#define __WATER_NTVDM_COM_H

#include "winbase.h"

struct dos_process;

BOOL dos_load_com(HANDLE file, struct dos_process *process);

#endif /* __WATER_NTVDM_COM_H */
