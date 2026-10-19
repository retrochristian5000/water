/* WHP Windows 95 RTM traditional audio-driver DDK boundary.
 * Win16 NE .DRV is separate from a Win9x i386 LE VxD or an NT/WDM .SYS.
 * Do not use Water's general Win32 mmddk.h as authenticated Win95 RTM DDK.
 * Reuse the existing Win16 compiler, width and far-pointer assertions.
 */
#ifndef WHP_WIN95_RTM_AUDIO_DDK_AUDIO_H
#define WHP_WIN95_RTM_AUDIO_DDK_AUDIO_H

#include <ddk/win95rtm/win16_abi.h>

#ifdef WHP_WIN95_AUDIO_USE_HISTORICAL_DDK
/* Caller must supply an independently verified historical DDK include path
 * before Watcom or Water includes, with the raw source provenance retained.
 */
#include <mmsystem.h>
#include <mmddk.h>
#endif

#endif /* WHP_WIN95_RTM_AUDIO_DDK_AUDIO_H */
