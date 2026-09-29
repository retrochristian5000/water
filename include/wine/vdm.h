/*
 * Water VDM personality contract.
 *
 * This process-local marker separates NT WOW from the DOS-based WIN386
 * personality.  It is intentionally small: lifecycle/state belongs to the
 * owning VDM implementation, not to KRNL386.
 */
#ifndef __WINE_VDM_H
#define __WINE_VDM_H

#define WATER_VDM_PERSONALITY_ENV    "WATER_VDM_PERSONALITY"
#define WATER_VDM_PERSONALITY_NT_WOW "nt-wow"

#endif /* __WINE_VDM_H */
