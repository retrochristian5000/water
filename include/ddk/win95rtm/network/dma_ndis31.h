/* Opt-in NDIS 3.x DMA allocation implementation declaration. */
#ifndef WHP_WIN95RTM_DMA_NDIS31_H
#define WHP_WIN95RTM_DMA_NDIS31_H
#ifndef WHP_WIN95_NDIS31_USE_HISTORICAL_DDK
#error "A verified NT 3.51 DDK is required"
#endif
#ifndef WHP_WIN95_NDIS31_DMA_ADAPTER_PROBE
#error "Historical DMA adapter must be an explicit compile probe"
#endif
#include "ddk/win95rtm/network/dma_policy.h"
void whp_ndis31_dma_ddk_allocation_ops(struct whp_ndis31_dma_ops *ops);
#endif
