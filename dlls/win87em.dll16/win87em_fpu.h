/*
 * Small host-x87 helpers for WIN87EM.DLL's Win16 __fpMath requests.
 *
 * These operate on the native x87 register stack, not a guest/TCG FPU
 * context. The host architecture fallback reports unsupported rather
 * than silently pretending that the guest register stack was changed.
 *
 * This header is private to win87em.dll16 and its portable tests.
 */
#ifndef __WATER_WIN87EM_FPU_H
#define __WATER_WIN87EM_FPU_H

#include <stdint.h>

#if defined(__i386__) || defined(__x86_64__)

static inline int win87em_round_st0(uint16_t rounding)
{
    uint16_t original, temporary;

    __asm__ __volatile__("fnstcw %0" : "=m" (original));
    temporary = (original & ~0x0c00) | (rounding & 0x0c00);
    __asm__ __volatile__("fldcw %0" : : "m" (temporary));
    __asm__ __volatile__("frndint");
    __asm__ __volatile__("fldcw %0" : : "m" (original));
    return 1;
}

static inline int win87em_pop_int32(uint16_t rounding, int32_t *result)
{
    uint16_t original, temporary;

    if (!result) return 0;
    __asm__ __volatile__("fnstcw %0" : "=m" (original));
    temporary = (original & ~0x0c00) | (rounding & 0x0c00);
    __asm__ __volatile__("fldcw %0" : : "m" (temporary));
    /* FISTP performs the requested rounding and pops exactly one ST0. */
    __asm__ __volatile__("fistpl %0" : "=m" (*result));
    __asm__ __volatile__("fldcw %0" : : "m" (original));
    return 1;
}

#else

static inline int win87em_round_st0(uint16_t rounding)
{
    (void)rounding;
    return 0;
}

static inline int win87em_pop_int32(uint16_t rounding, int32_t *result)
{
    (void)rounding;
    (void)result;
    return 0;
}

#endif
/* __WATER_WIN87EM_FPU_H */
#endif
