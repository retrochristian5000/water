/* Synthetic NDIS3 mock: validates the adapter's allocation and free calls.
 * This DOES NOT establish Windows 95 RTM or original NT 3.51 ABI validity.
 */
#include "ddk/win95rtm/network/dma_ndis31.h"
#include <ndis.h>
#include <stdio.h>
static unsigned char bytes[512];
static int allocations, frees, should_fail;
static int owner_cookie;
static NDIS_PHYSICAL_ADDRESS assigned;
static ULONG assigned_length;
static BOOLEAN assigned_cached;
void NdisMAllocateSharedMemory(NDIS_HANDLE owner, ULONG size,
    BOOLEAN cached, PVOID *virtual_address, PNDIS_PHYSICAL_ADDRESS physical_address)
{
    if (owner != &owner_cookie || cached || !virtual_address ||
        !physical_address || size != 512u) return;
    ++allocations;
    if (should_fail) { *virtual_address=0; return; }
    *virtual_address = bytes;
    physical_address->LowPart = 0x8000;
    physical_address->HighPart = 0;
}
void NdisMFreeSharedMemory(NDIS_HANDLE owner, ULONG size,
    BOOLEAN cached, PVOID virtual_address, NDIS_PHYSICAL_ADDRESS physical_address)
{
    if (owner != &owner_cookie || size != 512u || cached ||
        virtual_address != bytes || physical_address.LowPart != 0x8000 ||
        physical_address.HighPart) return;
    assigned=physical_address; assigned_length=size;assigned_cached=cached;
    ++frees;
}
int main(void)
{
    struct whp_ndis31_dma_ops ops = {0};
    void *cpu=(void *)1;
    uint32_t low=77, high=88;
    whp_ndis31_dma_ddk_allocation_ops(&ops);
    if (!ops.allocate || !ops.release || ops.sync_for_device || ops.sync_for_cpu) return 1;
    should_fail=1;
    if (ops.allocate(&owner_cookie,512,0,&cpu,&low,&high) || cpu || low || high || allocations!=1) return 2;
    should_fail=0;
    if (!ops.allocate(&owner_cookie,512,0,&cpu,&low,&high) ||
        cpu!=bytes || low!=0x8000 || high!=0 || allocations!=2) return 3;
    ops.release(&owner_cookie,512,0,cpu,low,high);
    if (frees!=1 || assigned.LowPart!=0x8000 || assigned_length!=512 || assigned_cached) return 4;
    puts("Synthetic NDIS 3.x DMA allocation adapter: 4 checks passed");
    return 0;
}
