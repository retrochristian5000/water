/*
 * Internal NT WOW32 dispatcher definitions.
 *
 * Keep this frame byte-packed: the layout mirrors the VDMFRAME built by
 * the NT WOW16CALL path.  It is shared only by Water's Win16/WOW bridge.
 */
#ifndef __WINE_WOW32_H
#define __WINE_WOW32_H

#include "windef.h"

#pragma pack(push,1)
typedef struct
{
    WORD  wTDB;
    WORD  wRetID;
    WORD  wLocalBP;
    WORD  wDI;
    WORD  wSI;
    WORD  wAX;
    WORD  wDX;
    WORD  wAppDS;
    WORD  wGS;
    WORD  wFS;
    WORD  wCX;
    WORD  wES;
    WORD  wBX;
    WORD  wBP;
    DWORD wThunkCSIP;
    DWORD wCallID;
    WORD  cbArgs;
    DWORD vpCSIP;
    BYTE  bArgs[1];
} WINEVDMFRAME;
#pragma pack(pop)

typedef DWORD (__cdecl *WOW32_DISPATCH_FRAME_PROC)(WINEVDMFRAME *);

#endif /* __WINE_WOW32_H */
