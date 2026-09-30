/*
 * Water NT WOW profile ABI for Win16 consumers.
 *
 * NTVDM selects the profile, WOW32 transports it as a process-local scalar,
 * and Win16 components such as KRNL386 consume it.  Keep these values out of
 * the public WOW thunk headers: they are Water-private NT WOW state.
 */
#ifndef __WINE_VDM16_H
#define __WINE_VDM16_H

/*
 * Keep these as explicit 32-bit scalar values rather than a C enum so the
 * NTVDM -> WOW32 -> Win16 bridge is stable across host compilers and
 * architectures.
 */
#define WATER_VDM_WOW_PROFILE_NONE  0x00000000u
#define WATER_VDM_WOW_PROFILE_NT31  0x00000310u
#define WATER_VDM_WOW_PROFILE_NT351 0x00000351u
#define WATER_VDM_WOW_PROFILE_NT5   0x00000500u

#endif /* __WINE_VDM16_H */
