/*
 * Water NT VDM/WOW personality-owner contract.
 *
 * Cross-family Win16 profile selection lives in wine/win16_profile.h.
 * The scalar NTVDM -> WOW32 -> Win16 profile ABI lives in wine/vdm16.h.
 * This header contains the identities owned by the Windows NT VDM/WOW line.
 */
#ifndef __WINE_VDM_H
#define __WINE_VDM_H

#include "wine/vdm16.h"

#define WATER_VDM_PERSONALITY_NT31_WOW  "nt31-wow"
#define WATER_VDM_PERSONALITY_NT351_WOW "nt351-wow"
#define WATER_VDM_PERSONALITY_NT5_WOW   "nt-wow"

/* Backward-compatible name for the current NT5-oriented NTVDM owner. */
#define WATER_VDM_PERSONALITY_NT_WOW WATER_VDM_PERSONALITY_NT5_WOW

#endif /* __WINE_VDM_H */
