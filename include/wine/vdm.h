/*
 * Water NT VDM/WOW personality contract.
 *
 * Cross-family Win16 profile selection lives in wine/win16_profile.h.
 * This header contains only identities owned by the Windows NT VDM/WOW line.
 */
#ifndef __WINE_VDM_H
#define __WINE_VDM_H

#define WATER_VDM_PERSONALITY_NT31_WOW  "nt31-wow"
#define WATER_VDM_PERSONALITY_NT351_WOW "nt351-wow"
#define WATER_VDM_PERSONALITY_NT5_WOW   "nt-wow"

/* Backward-compatible name for the current NT5-oriented NTVDM owner. */
#define WATER_VDM_PERSONALITY_NT_WOW WATER_VDM_PERSONALITY_NT5_WOW

#endif /* __WINE_VDM_H */
