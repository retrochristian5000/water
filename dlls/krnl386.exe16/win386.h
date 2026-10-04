/*
 * KRNL386 private bridge to the Windows/386 enhanced-mode host.
 *
 * The public shared-memory ABI lives in wine/win386.h.  Keep query helpers
 * private to KRNL386 so NTVDM and other personalities cannot accidentally
 * depend on KRNL386 implementation details.
 */
#ifndef __WINE_KRNL386_WIN386_H
#define __WINE_KRNL386_WIN386_H

#include "windef.h"
#include "wine/win386.h"

struct win386_session_info
{
    DWORD flags;
    WORD windows_version;
    WORD dos_version;
    BYTE dos_family;
    BYTE reserved;
    WORD current_vm;
    WORD system_vm;
};

extern BOOL WIN386_QuerySession( struct win386_session_info *info );

#endif /* __WINE_KRNL386_WIN386_H */
