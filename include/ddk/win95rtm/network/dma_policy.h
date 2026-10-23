/* Windows 95 RTM / NT 3.51 miniport DMA shared-memory ownership policy.
 * No device I/O, compiler-specific barriers, NDIS declarations, or OS calls.
 * Not thread-safe: a verified miniport must serialize all operations.
 */
#ifndef WHP_WIN95RTM_NDIS31_DMA_POLICY_H
#define WHP_WIN95RTM_NDIS31_DMA_POLICY_H
#include <stdint.h>

#define WHP_NDIS31_DMA_MAX_BYTES (1024u * 1024u)
#define WHP_NDIS31_DMA_TICKETS 8u

enum whp_ndis31_dma_status {
    WHP_NDIS31_DMA_OK = 0,
    WHP_NDIS31_DMA_BAD_ARG,
    WHP_NDIS31_DMA_BAD_STATE,
    WHP_NDIS31_DMA_ALLOC_FAILED,
    WHP_NDIS31_DMA_BAD_MAPPING,
    WHP_NDIS31_DMA_BUSY,
    WHP_NDIS31_DMA_SYNC_FAILED,
    WHP_NDIS31_DMA_EXHAUSTED
};

enum whp_ndis31_dma_state {
    WHP_NDIS31_DMA_EMPTY = 0,
    WHP_NDIS31_DMA_READY,
    WHP_NDIS31_DMA_QUIESCING
};

/* The callbacks are HOST platform services. Returning success from allocate
 * guarantees virtual and logical addresses describe a block that can be
 * freed using the exact same length, cached flag and address pair.
 * A FALSE return must not leave an allocation owned by the caller.
 * sync_for_device must prepare the given byte range and order writes before
 * the NIC's OWN bit; sync_for_cpu must acquire reads AFTER OWN clears.
 * Neither can be replaced with a no-op or just a compiler fence.
 */
struct whp_ndis31_dma_ops {
    int (*allocate)(void *owner, uint32_t length, int cached,
                    void **virtual_addr, uint32_t *bus_low, uint32_t *bus_high);
    void (*release)(void *owner, uint32_t length, int cached,
                    void *virtual_addr, uint32_t bus_low, uint32_t bus_high);
    int (*sync_for_device)(void *owner, void *address, uint32_t length, int from_device);
    int (*sync_for_cpu)(void *owner, void *address, uint32_t length, int from_device);
};

struct whp_ndis31_dma_ticket {
    uint32_t slot;
    uint32_t serial;
};

struct whp_ndis31_dma {
    enum whp_ndis31_dma_state state;
    struct whp_ndis31_dma_ops ops;
    void *owner;
    void *base;
    uint32_t length;
    uint32_t low;
    uint32_t high;
    uint32_t last_serial;
    uint32_t slot_serial[WHP_NDIS31_DMA_TICKETS];
    uint32_t slot_offset[WHP_NDIS31_DMA_TICKETS];
    uint32_t slot_length[WHP_NDIS31_DMA_TICKETS];
    uint8_t slot_state[WHP_NDIS31_DMA_TICKETS];
    uint32_t active;
    uint8_t cached;
};

/* Structure MUST initially be zero-initialized. Reinitializing a live or
 * previously used block is rejected to preserve serial/ownership history. */
enum whp_ndis31_dma_status whp_ndis31_dma_construct(struct whp_ndis31_dma *dma);

/* alignment must be power-of-two <= 4096, and the returned logical bus
 * address must be within 32 bits and aligned; no CPU address translation. */
enum whp_ndis31_dma_status whp_ndis31_dma_allocate(
    struct whp_ndis31_dma *dma, const struct whp_ndis31_dma_ops *ops,
    void *owner, uint32_t length, uint32_t alignment, int cached);

/* IO submission locks a specific range until completion AND upper-layer ACK.
 * from_device=1 for NIC -> host receive; 0 for host -> NIC transmit. */
enum whp_ndis31_dma_status whp_ndis31_dma_begin(
    struct whp_ndis31_dma *dma, uint32_t offset, uint32_t length,
    int from_device, struct whp_ndis31_dma_ticket *ticket,
    uint32_t *logical_bus);
enum whp_ndis31_dma_status whp_ndis31_dma_complete(
    struct whp_ndis31_dma *dma, struct whp_ndis31_dma_ticket ticket);
enum whp_ndis31_dma_status whp_ndis31_dma_ack(
    struct whp_ndis31_dma *dma, struct whp_ndis31_dma_ticket ticket);

/* Caller must first independently STOP bus-mastering/IRQ/DMA; QUIESCING is
 * bookkeeping only. Outstanding I/O blocks release until acknowledged. */
enum whp_ndis31_dma_status whp_ndis31_dma_quiesce(struct whp_ndis31_dma *dma);
enum whp_ndis31_dma_status whp_ndis31_dma_release(struct whp_ndis31_dma *dma);
#endif
