/*
 * NTVDM DOS-emulation-manager disk services.
 */
#ifndef __WATER_NTVDM_DEM_DISK_H
#define __WATER_NTVDM_DEM_DISK_H

#include "windef.h"

BOOL DEM_AbsoluteRead(BYTE drive, DWORD begin, DWORD nr_sect, BYTE *dataptr,
                      BOOL fake_success);
BOOL DEM_AbsoluteWrite(BYTE drive, DWORD begin, DWORD nr_sect,
                       const BYTE *dataptr, BOOL fake_success);

#endif /* __WATER_NTVDM_DEM_DISK_H */
