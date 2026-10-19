/* WHP Windows 95 RTM Win16 compile boundary (audio and display) (not a DDK copy).
 * Intentionally NO DIBENGINE/GDIINFO/PDEVICE layout or DIBENG exports here.
 * The DDK owns those definitions, and Win16 uses segmented pointers.
 */
#ifndef WHP_WIN95_RTM_WIN16_ABI_H
#define WHP_WIN95_RTM_WIN16_ABI_H

#if !defined(__WATCOMC__) || defined(__386__) || defined(_WIN32)
#error "Use the OpenWatcom 16-bit wcc compiler with -bt=windows, not wcc386 or Clang"
#endif

#include <windows.h>

#define WHP_WIN95_RTM_DISPLAY_VERSION 0x0400u

/* Compile-time checks: 16-bit Windows has 16-bit int/WORD, 32-bit
 * long/DWORD, and 16:16 far pointers. This file is a compile probe,
 * not an implementation of the Windows 95 GDI DDI.
 */
typedef char whp_w95_win16_int_is_16[(sizeof(int) == 2) ? 1 : -1];
typedef char whp_w95_win16_word_is_16[(sizeof(WORD) == 2) ? 1 : -1];
typedef char whp_w95_win16_long_is_32[(sizeof(long) == 4) ? 1 : -1];
typedef char whp_w95_win16_dword_is_32[(sizeof(DWORD) == 4) ? 1 : -1];
typedef char whp_w95_win16_far_is_32[(sizeof(void __far *) == 4) ? 1 : -1];
typedef char whp_w95_win16_lpstr_is_far[(sizeof(LPSTR) == 4) ? 1 : -1];

#endif /* WHP_WIN95_RTM_WIN16_ABI_H */
