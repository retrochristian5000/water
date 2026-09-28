/*
 * Darwin host ABI helpers for the Mac driver.
 *
 * Keep pointer-authentication policy at the native Darwin boundary.  Wine/PE
 * callback addresses transported as integers are guest ABI values and must not
 * be treated as native arm64e C function pointers here.
 */

#ifndef __WINE_MACDRV_DARWIN_ABI_H
#define __WINE_MACDRV_DARWIN_ABI_H

#include <dlfcn.h>

/*
 * arm64e indirect calls require compiler-managed pointer authentication.
 * Reject an arm64e compilation that does not actually enable that ABI instead
 * of silently producing an arm64-compatible Unix driver.
 */
#if defined(__arm64e__) && (!defined(__clang__) || !defined(__PTRAUTH__))
# error winemac.drv arm64e requires Clang pointer-authentication support
#endif
#if defined(__arm64e__) && defined(__clang__)
# if !__has_feature(ptrauth_calls)
#  error winemac.drv arm64e requires authenticated indirect calls
# endif
#endif

/*
 * On arm64e, Darwin dlsym() returns code symbols already signed using the
 * default C function-pointer schema.  Convert directly to the destination
 * function-pointer type; do not strip, re-sign, or round-trip through an
 * integer.
 */
#if defined(__clang__) || defined(__GNUC__)
# define MACDRV_DLSYM_FUNCTION(target, handle, symbol) \
    ((target) = (__typeof__(target))dlsym((handle), (symbol)))
#else
# define MACDRV_DLSYM_FUNCTION(target, handle, symbol) \
    ((target) = dlsym((handle), (symbol)))
#endif

/*
 * The OpenGL driver interface historically transports procedure addresses as
 * void *.  Clang's arm64e ABI preserves a C function pointer's authentication
 * signature across this direct cast.  Keep the conversion direct.
 */
#define MACDRV_FUNCTION_POINTER_AS_DATA(function) ((void *)(function))

#endif /* __WINE_MACDRV_DARWIN_ABI_H */
