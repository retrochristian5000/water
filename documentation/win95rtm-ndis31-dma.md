# Windows 95 RTM NDIS 3.1 DMA shared-memory boundary

Status (2026-10-23): portable memory-ownership logic and a synthetic NDIS 3.x allocation fixture tested. **Authentic NT 3.51 DDK ABI, Windows 95 runtime, PCI bus mastering and DMA cache barriers are not verified.**

## Implemented

- `include/ddk/win95rtm/network/dma_policy.h` and `libs/whp-ndis31/dma_policy.c` implement a 1 MiB maximum block, 32-bit DMA address validation, power-of-two alignment (up to 4096), explicit CPU virtual versus logical bus addresses, exact allocation/free lifetime, and eight non-overlapping DMA tickets.
- DMA uses **begin -> complete -> upper-layer ACK**, with separate cache-sync callbacks. Quiescing blocks new work; pending or CPU-held transfers block release. Tickets never reuse serials across allocation/free cycles. Invalid mapping, overlap, sync failure, live reinitialization and serial exhaustion fail closed.
- `libs/whp-ndis31/dma_ndis31.c` is an opt-in **DDK compile-only adapter**. It uses the supplied historical `ndis.h` types to call `NdisMAllocateSharedMemory` and `NdisMFreeSharedMemory` and is gated by `WHP_WIN95_NDIS31_USE_HISTORICAL_DDK` plus `WHP_WIN95_NDIS31_DMA_ADAPTER_PROBE`. Neither the original signatures nor import symbols are authenticated without the DDK.
- The DDK adapter intentionally leaves `sync_for_device` and `sync_for_cpu` NULL; the portable allocator refuses to proceed until both are supplied by a verified platform implementation. No no-op callbacks, PCI register writes, bus-master activation, interrupt control or hardware DMA are provided.

## Scope guards

These functions are NOT linked into Water's newer Wine-like `dlls/ndis.sys`. Its DMA symbols are stubs and do not implement Windows 95 `NDIS.VXD`. Water's later `include/ddk/ndis.h` is not the RTM miniport header. Never import WDM, NDIS 4/5/6, `NET_BUFFER_LIST`, or a modern NDIS 6 DMA contract into this path. The consumer must serialize all calls, verify that the NIC stopped before `quiesce`/`release`, and confirm device OWN-bit ordering and CPU cache synchronization independently.

## Regression

Run `sh tests/test-ndis31-dma.sh` from Water's root. This executes portable host checks, a **synthetic** NDIS header adapter test (fixture only under `tests/fixtures/ndis31-synthetic`), opt-in failure rejection, and an i386 Windows COFF compile when Clang is installed. The fake header is NOT a real DDK, and no miniport is loaded.

For an additional runtime check: `clang -std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude libs/whp-ndis31/dma_policy.c tests/ndis31-dma-policy.c -o /tmp/whp-ndis31-dma-test && /tmp/whp-ndis31-dma-test`.

## Evidence / provenance

- Windows 95 DDK miniport recommendation for NT 3.51: https://techshelps.github.io/MSDN/WIN95DDK/HTML/S2D14.HTM
- Microsoft KB Q124079 (NT 3.1/3.5/3.51 DDK) concerns `NdisAllocateMemory` noncached allocation during initialization, **not** proof of shared-memory DMA ABI: https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/124079
- Current `NdisMAllocateSharedMemory`/`NdisMFreeSharedMemory` documentation is used for conceptual comparison only; its NDIS 6.x requirements do not establish NT 3.51 support: https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ndis/nf-ndis-ndismallocatesharedmemory
