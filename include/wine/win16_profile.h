/*
 * Water Win16 guest profile selection.
 *
 * These process-local markers describe cross-family Win16 guest choices.
 * They are shared by DOS Windows, Win9x and NT WOW, and do not describe
 * WIN386 session ownership or NTVDM-specific state.
 *
 * Keep the existing environment-variable strings for compatibility with
 * launch scripts; only their C-side ownership is corrected here.
 */
#ifndef __WINE_WIN16_PROFILE_H
#define __WINE_WIN16_PROFILE_H

#define WATER_WIN16_PERSONALITY_ENV            "WATER_VDM_PERSONALITY"
#define WATER_WIN16_PERSONALITY_WIN30_STANDARD "win30-standard"
#define WATER_WIN16_PERSONALITY_WIN31_STANDARD "win31-standard"
#define WATER_WIN16_PERSONALITY_WIN95_OSR2     "win95-osr2"

#define WATER_WIN16_KERNEL_ENV                  "WATER_KERNEL16_IMAGE"
#define WATER_WIN16_KERNEL_KRNL286              "krnl286"
#define WATER_WIN16_KERNEL_KRNL386              "krnl386"
#define WATER_WIN16_CPU_LEVEL_ENV               "WATER_X86_CPU_LEVEL"

#endif /* __WINE_WIN16_PROFILE_H */
