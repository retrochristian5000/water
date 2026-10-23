/* SYNTHETIC ABI fixture ONLY. Not Microsoft DDK; never use as real driver ABI. */
#ifndef WHP_MOCK_NDIS_H
#define WHP_MOCK_NDIS_H
#include <stdint.h>
typedef void *NDIS_HANDLE;
typedef void *PVOID;
typedef uint32_t ULONG;
typedef int32_t LONG;
typedef uint8_t BOOLEAN;
#define TRUE 1
#define FALSE 0
typedef struct _NDIS_PHYSICAL_ADDRESS {
    ULONG LowPart;
    LONG HighPart;
} NDIS_PHYSICAL_ADDRESS, *PNDIS_PHYSICAL_ADDRESS;
void NdisMAllocateSharedMemory(NDIS_HANDLE handle, ULONG length,
                               BOOLEAN cached, PVOID *virtual_address,
                               PNDIS_PHYSICAL_ADDRESS physical_address);
void NdisMFreeSharedMemory(NDIS_HANDLE handle, ULONG length,
                           BOOLEAN cached, PVOID virtual_address,
                           NDIS_PHYSICAL_ADDRESS physical_address);
#endif
