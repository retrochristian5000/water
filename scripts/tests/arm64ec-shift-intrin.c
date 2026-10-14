/* ARM64EC must implement the x64 128-bit shift interface inline.
 * Include only intrin.h to catch provider-header gaps masked by winnt.h. */
#ifdef WHP_EXISTING_SHIFT_ALIAS
/* Model a prior WinNT __shiftright128 -> ShiftRight128 macro mapping. */
unsigned __int64 ShiftRight128(unsigned __int64, unsigned __int64,
                              unsigned char);
#define __shiftright128 ShiftRight128
#endif

#include <intrin.h>

#ifndef WHP_EXISTING_SHIFT_ALIAS
unsigned __int64 water_shift_right(unsigned __int64 low, unsigned __int64 high,
                                   unsigned char count)
{
    return __shiftright128(low, high, count);
}
#endif

unsigned __int64 water_shift_left(unsigned __int64 low, unsigned __int64 high,
                                  unsigned char count)
{
    return __shiftleft128(low, high, count);
}
