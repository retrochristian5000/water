/* Genuine-NT-3.51-DDK-only shared DMA allocation adapter.
 * Not compiled by Water's contemporary NDIS.SYS or normal Win32 build.
 * Header fingerprints and original import/link library must be verified by
 * the consumer. No historical DDK header is vendored or recreated here.
 */
#ifndef WHP_WIN95_NDIS31_USE_HISTORICAL_DDK
#error "NT 3.51 NDIS header provenance gate is required"
#endif
#ifndef WHP_WIN95_NDIS31_DMA_ADAPTER_PROBE
#error "NDIS 3.1 DMA adapter is explicit DDK compile-only until validated"
#endif
#ifndef NDIS_MINIPORT_DRIVER
#define NDIS_MINIPORT_DRIVER 1
#endif
#include "ddk/win95rtm/network/ndis31.h"
#include "ddk/win95rtm/network/dma_ndis31.h"

/* The NDIS_HANDLE here MUST be the NDIS miniport adapter handle returned
 * during actual initialization; never substitute a wrapper or PCI pointer. */
static int whp_ndis31_ddk_allocate(void *owner, uint32_t length, int cached,
    void **virtual_addr, uint32_t *low, uint32_t *high)
{
    NDIS_PHYSICAL_ADDRESS address;
    PVOID virtual_base = (PVOID)0;
    NDIS_HANDLE handle = (NDIS_HANDLE)owner;
    if (!handle || !virtual_addr || !low || !high || !length)
        return 0;
    *virtual_addr = (void *)0;
    *low = 0;
    *high = 0;
    address.LowPart = 0;
    address.HighPart = 0;
    NdisMAllocateSharedMemory(handle, (ULONG)length,
                              cached ? TRUE : FALSE,
                              &virtual_base, &address);
    if (!virtual_base) return 0;
    *virtual_addr = virtual_base;
    *low = address.LowPart;
    *high = (uint32_t)address.HighPart;
    return 1;
}

static void whp_ndis31_ddk_release(void *owner, uint32_t length, int cached,
    void *virtual_addr, uint32_t low, uint32_t high)
{
    NDIS_PHYSICAL_ADDRESS address;
    address.LowPart = low;
    address.HighPart = (LONG)high;
    NdisMFreeSharedMemory((NDIS_HANDLE)owner, (ULONG)length,
                          cached ? TRUE : FALSE, (PVOID)virtual_addr, address);
}

/* Deliberately leaves synchronization callbacks NULL. Historical DDK
 * cache/DMA ordering must be independently verified for the real device;
 * a consumer cannot whp_ndis31_dma_allocate() with this ops table alone. */
void whp_ndis31_dma_ddk_allocation_ops(struct whp_ndis31_dma_ops *ops)
{
    if (!ops) return;
    ops->allocate = whp_ndis31_ddk_allocate;
    ops->release = whp_ndis31_ddk_release;
    ops->sync_for_device = 0;
    ops->sync_for_cpu = 0;
}
