/* Windows 95 RTM i386 VxD ABI baseline; NOT a complete VMM SDK.
 * Source controls: Windows 95 DDK "Building a Virtual Device" (DDB @1),
 * R. Asche, "What's New in Windows 95 for VxD Writers?" (1995), and
 * cross-checked DDB fields/control IDs against JHRobotics/vmdisp9x vmm.h.
 * Do not substitute Water's NTDDK/WDM interfaces for this ABI.
 */
#ifndef WHP_WIN95RTM_VMM_H
#define WHP_WIN95RTM_VMM_H

#if !defined(__i386__) && !defined(_M_IX86)
#error "Windows 95 RTM VxD DDB layout requires an i386 target"
#endif

#include <stddef.h>
#include <stdint.h>

#define WHP_WIN95_DDK_VERSION             0x0400u
#define WHP_WIN95_VMM_DEVICE_ID           0x0001u
#define WHP_WIN95_UNDEFINED_DEVICE_ID     0x0000u
#define WHP_WIN95_UNDEFINED_INIT_ORDER    0x80000000u
#define WHP_WIN95_SYS_DYNAMIC_DEVICE_INIT 0x001bu
#define WHP_WIN95_SYS_DYNAMIC_DEVICE_EXIT 0x001cu

/* The control entry is called by VMM using register/flags conventions,
 * NOT the ordinary C calling convention. Implement it using a naked
 * assembly entry until the ABI and loader are validated in a Win95 RTM VM.
 */
#pragma pack(push, 1)
struct whp_win95_vxd_ddb {
    uint32_t next;
    uint16_t sdk, id;
    uint8_t major, minor;
    uint16_t flags;
    uint8_t name[8];
    uint32_t init_order;
    void (*control)(void);
    uint32_t v86, pm, v86_csip, pm_csip, ref_data;
    uint32_t services, services_count, win32_services, prev, ddb_size;
    uint32_t reserved1, reserved2, reserved3;
};
#pragma pack(pop)

#if defined(__cplusplus)
static_assert(sizeof(struct whp_win95_vxd_ddb) == 80, "Win95 VxD DDB must be 80 bytes");
static_assert(offsetof(struct whp_win95_vxd_ddb, control) == 24, "Win95 VxD DDB control offset");
static_assert(offsetof(struct whp_win95_vxd_ddb, ddb_size) == 64, "Win95 VxD DDB size-field offset");
#else
_Static_assert(sizeof(struct whp_win95_vxd_ddb) == 80, "Win95 VxD DDB must be 80 bytes");
_Static_assert(offsetof(struct whp_win95_vxd_ddb, control) == 24, "Win95 VxD DDB control offset");
_Static_assert(offsetof(struct whp_win95_vxd_ddb, ddb_size) == 64, "Win95 VxD DDB size-field offset");
#endif

#endif /* WHP_WIN95RTM_VMM_H */
