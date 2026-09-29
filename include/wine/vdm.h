/*
 * Water VDM personality contract.
 *
 * These process-local markers separate versioned Win3.x standard-mode,
 * Win9x and NT WOW environments from the DOS-based WIN386 personality.
 * Kernel image and guest CPU selection are orthogonal to the Windows mode:
 * KRNL286 is 286-compatible, KRNL386 requires a 386+, and either image can
 * service Windows 3.x standard mode on suitable hardware.
 */
#ifndef __WINE_VDM_H
#define __WINE_VDM_H

#define WATER_VDM_PERSONALITY_ENV            "WATER_VDM_PERSONALITY"
#define WATER_VDM_PERSONALITY_WIN30_STANDARD "win30-standard"
#define WATER_VDM_PERSONALITY_WIN31_STANDARD "win31-standard"
#define WATER_VDM_PERSONALITY_WIN95_OSR2     "win95-osr2"
#define WATER_VDM_PERSONALITY_NT31_WOW       "nt31-wow"
#define WATER_VDM_PERSONALITY_NT351_WOW      "nt351-wow"
#define WATER_VDM_PERSONALITY_NT5_WOW        "nt-wow"

#define WATER_VDM_KERNEL16_ENV                "WATER_KERNEL16_IMAGE"
#define WATER_VDM_KERNEL16_KRNL286            "krnl286"
#define WATER_VDM_KERNEL16_KRNL386            "krnl386"
#define WATER_VDM_X86_CPU_LEVEL_ENV           "WATER_X86_CPU_LEVEL"

/* Backward-compatible name for the current NT5-oriented NTVDM owner. */
#define WATER_VDM_PERSONALITY_NT_WOW WATER_VDM_PERSONALITY_NT5_WOW

#endif /* __WINE_VDM_H */
