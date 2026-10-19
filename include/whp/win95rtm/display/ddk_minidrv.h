/* Opt-in inclusion of historical Win95 display DDK interfaces.
 * Neither Water's Win32 API nor modern NT winddi.h is a replacement.
 * Do not guess DIBENGINE or GDIINFO layouts and export calling conventions.
 */
#ifndef WHP_WIN95_RTM_DISPLAY_DDK_MINIDRV_H
#define WHP_WIN95_RTM_DISPLAY_DDK_MINIDRV_H

#include <whp/win95rtm/display/win16_abi.h>

#if !defined(WHP_WIN95_DISPLAY_USE_HISTORICAL_DDK)
#error "Historical Windows 95 display DDK inclusion requires an explicit opt-in"
#endif

/* These must resolve from separately acquired, version-checked DDK sources. */
#include <gdidefs.h>
#include <dibeng.h>

/* MINI-VDD is a distinct 32-bit VxD interface. The Win95 RTM DDK
 * documents MINIVDD.INC; do not force a possibly later minivdd.h here.
 */
#endif /* WHP_WIN95_RTM_DISPLAY_DDK_MINIDRV_H */
