#include "ddk/win95rtm/network/dma_policy.h"
#include <stddef.h>

static void clear_fields(struct whp_ndis31_dma *d)
{
    unsigned i;
    d->state = WHP_NDIS31_DMA_EMPTY;
    d->owner = NULL;
    d->base = NULL;
    d->length = d->low = d->high = d->active = 0;
    d->last_serial = 0;
    d->cached = 0;
    d->ops.allocate = NULL;
    d->ops.release = NULL;
    d->ops.sync_for_device = NULL;
    d->ops.sync_for_cpu = NULL;
    for (i = 0; i < WHP_NDIS31_DMA_TICKETS; ++i) {
        d->slot_serial[i] = 0;
        d->slot_offset[i] = 0;
        d->slot_length[i] = 0;
        d->slot_state[i] = 0;
    }
}

enum whp_ndis31_dma_status whp_ndis31_dma_construct(struct whp_ndis31_dma *d)
{
    if (!d) return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_EMPTY || d->base || d->active || d->last_serial)
        return WHP_NDIS31_DMA_BAD_STATE;
    clear_fields(d);
    return WHP_NDIS31_DMA_OK;
}

static int align_ok(uint32_t align)
{
    return align != 0 && align <= 4096u && (align & (align - 1u)) == 0;
}

enum whp_ndis31_dma_status whp_ndis31_dma_allocate(
    struct whp_ndis31_dma *d, const struct whp_ndis31_dma_ops *ops,
    void *owner, uint32_t size, uint32_t alignment, int cached)
{
    void *virtual_addr = NULL;
    uint32_t low = 0, high = 0;
    int ok;
    if (!d || !ops || !ops->allocate || !ops->release ||
        !ops->sync_for_device || !ops->sync_for_cpu ||
        !owner || size == 0 || size > WHP_NDIS31_DMA_MAX_BYTES ||
        !align_ok(alignment) || (cached != 0 && cached != 1))
        return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_EMPTY || d->active || d->base)
        return WHP_NDIS31_DMA_BAD_STATE;
    ok = ops->allocate(owner, size, cached, &virtual_addr, &low, &high);
    if (!ok) {
        /* Callback failure contract: it retains any partial allocation. */
        return WHP_NDIS31_DMA_ALLOC_FAILED;
    }
    if (!virtual_addr) return WHP_NDIS31_DMA_BAD_MAPPING;
    if (high != 0 || low == 0 || (low & (alignment - 1u)) != 0 ||
        (uint64_t)low + size - 1u > UINT32_MAX) {
        /* Successful allocator returned a complete release tuple. */
        ops->release(owner, size, cached, virtual_addr, low, high);
        return WHP_NDIS31_DMA_BAD_MAPPING;
    }
    d->ops = *ops;
    d->owner = owner;
    d->base = virtual_addr;
    d->length = size;
    d->low = low;
    d->high = high;
    d->cached = (uint8_t)cached;
    d->state = WHP_NDIS31_DMA_READY;
    return WHP_NDIS31_DMA_OK;
}

static int ticket_matches(const struct whp_ndis31_dma *d,
    struct whp_ndis31_dma_ticket t)
{
    return t.slot < WHP_NDIS31_DMA_TICKETS && t.serial != 0 &&
        d->slot_serial[t.slot] == t.serial;
}

enum whp_ndis31_dma_status whp_ndis31_dma_begin(
    struct whp_ndis31_dma *d, uint32_t offset, uint32_t length,
    int from_device, struct whp_ndis31_dma_ticket *out, uint32_t *bus)
{
    unsigned i;
    uint8_t *address;
    uint32_t logical;
    if (!d || !out || !bus || length == 0 ||
        (from_device != 0 && from_device != 1))
        return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_READY)
        return WHP_NDIS31_DMA_BAD_STATE;
    if (offset > d->length || length > d->length - offset)
        return WHP_NDIS31_DMA_BAD_ARG;
    if (d->active >= WHP_NDIS31_DMA_TICKETS)
        return WHP_NDIS31_DMA_BUSY;
    /* Shared DMA memory can be simultaneously assigned to multiple
     * operations only when their ranges do not overlap (including CPU-held). */
    for (i = 0; i < WHP_NDIS31_DMA_TICKETS; ++i) {
        if (d->slot_state[i] && offset < d->slot_offset[i] + d->slot_length[i] &&
            d->slot_offset[i] < offset + length)
            return WHP_NDIS31_DMA_BUSY;
    }
    if (d->last_serial == UINT32_MAX)
        return WHP_NDIS31_DMA_EXHAUSTED;
    for (i = 0; i < WHP_NDIS31_DMA_TICKETS; ++i)
        if (d->slot_state[i] == 0) break;
    if (i == WHP_NDIS31_DMA_TICKETS)
        return WHP_NDIS31_DMA_BUSY;
    /* Successful common-buffer allocation established a 32-bit logical
     * address range, with independent host VA; never cast host VA to DMA. */
    logical = d->low + offset;
    address = (uint8_t *)d->base + offset;
    if (!d->ops.sync_for_device(d->owner, address, length, from_device))
        return WHP_NDIS31_DMA_SYNC_FAILED;
    ++d->last_serial;
    d->slot_serial[i] = d->last_serial;
    d->slot_offset[i] = offset;
    d->slot_length[i] = length;
    d->slot_state[i] = (uint8_t)(from_device ? 2 : 1);
    ++d->active;
    out->slot = i;
    out->serial = d->last_serial;
    *bus = logical;
    return WHP_NDIS31_DMA_OK;
}

enum whp_ndis31_dma_status whp_ndis31_dma_complete(
    struct whp_ndis31_dma *d, struct whp_ndis31_dma_ticket ticket)
{
    unsigned i;
    if (!d) return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_READY &&
        d->state != WHP_NDIS31_DMA_QUIESCING)
        return WHP_NDIS31_DMA_BAD_STATE;
    if (!ticket_matches(d, ticket)) return WHP_NDIS31_DMA_BAD_ARG;
    i = ticket.slot;
    if (d->slot_state[i] != 1 && d->slot_state[i] != 2)
        return WHP_NDIS31_DMA_BAD_STATE;
    /* Caller already acquired cleared OWN with a validated platform
     * read barrier. This callback must make completion data CPU-visible. */
    if (!d->ops.sync_for_cpu(d->owner,
                             (uint8_t *)d->base + d->slot_offset[i],
                             d->slot_length[i], d->slot_state[i] == 2))
        return WHP_NDIS31_DMA_SYNC_FAILED;
    d->slot_state[i] = 3;
    return WHP_NDIS31_DMA_OK;
}

enum whp_ndis31_dma_status whp_ndis31_dma_ack(
    struct whp_ndis31_dma *d, struct whp_ndis31_dma_ticket ticket)
{
    unsigned i;
    if (!d) return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_READY &&
        d->state != WHP_NDIS31_DMA_QUIESCING)
        return WHP_NDIS31_DMA_BAD_STATE;
    if (!ticket_matches(d, ticket)) return WHP_NDIS31_DMA_BAD_ARG;
    i = ticket.slot;
    if (d->slot_state[i] != 3 || d->active == 0)
        return WHP_NDIS31_DMA_BAD_STATE;
    d->slot_state[i] = 0;
    d->slot_serial[i] = 0;
    d->slot_offset[i] = 0;
    d->slot_length[i] = 0;
    --d->active;
    return WHP_NDIS31_DMA_OK;
}

enum whp_ndis31_dma_status whp_ndis31_dma_quiesce(struct whp_ndis31_dma *d)
{
    if (!d) return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_READY)
        return WHP_NDIS31_DMA_BAD_STATE;
    d->state = WHP_NDIS31_DMA_QUIESCING;
    return WHP_NDIS31_DMA_OK;
}

enum whp_ndis31_dma_status whp_ndis31_dma_release(struct whp_ndis31_dma *d)
{
    void *owner, *base;
    uint32_t length, low, high, serial;
    int cached;
    void (*release)(void *, uint32_t, int, void *, uint32_t, uint32_t);
    if (!d) return WHP_NDIS31_DMA_BAD_ARG;
    if (d->state != WHP_NDIS31_DMA_QUIESCING)
        return WHP_NDIS31_DMA_BAD_STATE;
    if (d->active) return WHP_NDIS31_DMA_BUSY;
    owner = d->owner; base = d->base; length = d->length;
    low = d->low; high = d->high; cached = d->cached;
    release = d->ops.release;
    serial = d->last_serial;
    /* No reentrant callback can observe a still-allocated block. Caller
     * MUST independently quiesce NIC DMA before entering this function. */
    clear_fields(d);
    /* Old completion tickets must NEVER match a later allocation. */
    d->last_serial = serial;
    release(owner, length, cached, base, low, high);
    return WHP_NDIS31_DMA_OK;
}
