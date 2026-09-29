/*
 * Water VDM personality contract.
 *
 * These process-local markers separate versioned NT WOW environments from
 * the DOS-based WIN386 personality.  Lifecycle/state belongs to the owning
 * VDM implementation, not to KRNL386.
 */
#ifndef __WINE_VDM_H
#define __WINE_VDM_H

#define WATER_VDM_PERSONALITY_ENV       "WATER_VDM_PERSONALITY"
#define WATER_VDM_PERSONALITY_NT351_WOW "nt351-wow"
#define WATER_VDM_PERSONALITY_NT5_WOW   "nt-wow"

/* Backward-compatible name for the current NT5-oriented NTVDM owner. */
#define WATER_VDM_PERSONALITY_NT_WOW WATER_VDM_PERSONALITY_NT5_WOW

#endif /* __WINE_VDM_H */
