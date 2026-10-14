/* ARM64EC must lower the x64-compatible _umul128 without a CRT call.
 * Compile with the Water and pinned LLVM intrinsic-header providers. */
#ifdef WHP_EXISTING_MUL_ALIAS
/* Model WinNT's _umul128 -> UnsignedMultiply128 macro before <intrin.h>. */
unsigned __int64 UnsignedMultiply128(unsigned __int64, unsigned __int64,
                                    unsigned __int64 *);
#define _umul128 UnsignedMultiply128
#endif
#include <intrin.h>

unsigned __int64 water_mulwide_probe(unsigned __int64 a, unsigned __int64 b,
                                    unsigned __int64 *high)
{
    return _umul128(a, b, high);
}
