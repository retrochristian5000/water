/*
 * Water Windows/386 Enhanced Mode session ABI.
 *
 * This state belongs only to the DOS-based Windows 3.x enhanced-mode
 * personality.  NTVDM must not consume it.
 */
#ifndef __WINE_WIN386_H
#define __WINE_WIN386_H

#include "windef.h"

#define WATER_WIN386_SESSION_ENV "WATER_WIN386_SESSION"
#define WATER_WIN386_VM_ENV      "WATER_WIN386_VM"

#define WATER_WIN386_MAGIC       0x36383357  /* "W386" */
#define WATER_WIN386_ABI_VERSION 1

#define WATER_WIN386_VM_SYSTEM   1

#define WATER_WIN386_VERSION_30  MAKEWORD(3, 0)
#define WATER_WIN386_VERSION_31  MAKEWORD(3, 10)

#define WATER_WIN386_FLAG_ACTIVE 0x00000001
#define WATER_WIN386_FLAG_VMM    0x00000002

struct water_win386_session
{
    DWORD magic;
    DWORD abi_version;
    DWORD flags;
    DWORD owner_pid;
    WORD windows_mux_version;
    WORD system_vm;
    LONG next_vm;
    LONG active_vms;
};

#endif /* __WINE_WIN386_H */
