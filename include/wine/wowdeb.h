/*
 * WOWDEB Win16 debugging-helper protocol.
 *
 * The communication block layout matches the Windows NT WOWDEB/VDMDBG
 * contract.  Keep this structure shared between the Win16 helper and any
 * future VDMDBG remote transport implementation.
 */
#ifndef __WINE_WOWDEB_H
#define __WINE_WOWDEB_H

#include "windef.h"

#define WOWDEB_COMM_BLOCK_SIZE 4096
#define WOWDEB_DEAD_VALUE      0xfefefefe

typedef struct
{
    DWORD dwBlockAddress;
    DWORD dwReturnValue;
    WORD  wArgsPassed;
    WORD  wArgsSize;
    WORD  wBlockLength;
    WORD  wSuccess;
} WOWDEB_COM_HEADER;

#endif /* __WINE_WOWDEB_H */
