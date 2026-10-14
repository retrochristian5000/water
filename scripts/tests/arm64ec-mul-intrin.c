/* ARM64EC must lower the x64-compatible _umul128 without a CRT call.
 * Compile with the Water and pinned LLVM intrinsic-header providers. */
#include <intrin.h>

unsigned __int64 water_mulwide_probe(unsigned __int64 a, unsigned __int64 b,
                                    unsigned __int64 *high)
{
    return _umul128(a, b, high);
}
