/* Windows 95 RTM NDIS 3.1 Ethernet miniport DDK boundary.
 * Microsoft's Win95 DDK directs miniport authors to the NT 3.51 DDK.
 * This header defines NO guessed NDIS structs, VxD services or imports.
 * Full NDIS MAC VxDs and NDIS 2 drivers are distinct targets.
 */
#ifndef WHP_WIN95RTM_NDIS31_H
#define WHP_WIN95RTM_NDIS31_H

#if !defined(__i386__) && !defined(_M_IX86) && !defined(__386__)
#error "Windows 95 RTM NDIS 3.1 network drivers require an x86 32-bit compiler"
#endif

#ifdef WHP_WIN95_NDIS31_USE_HISTORICAL_DDK
/* Verified NT 3.51 DDK include path must precede all Water/host headers.
 * In particular, Water's later include/ddk/ndis.h is not an RTM DDK.
 */
#include <ndis.h>
#endif

#endif /* WHP_WIN95RTM_NDIS31_H */
