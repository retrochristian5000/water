/*
 * DOS interrupt 34->3e handlers.  All FPU interrupt code should be
 * moved into this file.
 *
 * Copyright 2002 Robert 'Admiral' Coeyman
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>
#include "dosexe.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(int);

/*
 *  The actual work is done by a single routine.
 */

static void FPU_ModifyCode(I386_CONTEXT *context, BYTE Opcode);

/* Microsoft linked 8087 escape instructions as INT 34h-3Ch and FWAIT as
 * INT 3Dh. Restore bytes only at a verified INT site within the code segment;
 * the handler may also be called by an unrelated or malformed guest vector.
 * This is the hardware-FPU fast path, not a software 8087 emulator. */
static BYTE *FPU_GetPatchSite(I386_CONTEXT *context, BYTE vector, unsigned int following)
{
    WORD sel = context->SegCs;
    DWORD ip;
    BYTE *code;

    if (!ldt_is_system(sel) && !ldt_is_valid(sel))
    {
        WARN("x87 INT %02x has an invalid code selector %04x\n", vector, sel);
        return NULL;
    }
    ip = ldt_is_32bit(sel) ? context->Eip : LOWORD(context->Eip);
    /* INT is two bytes. INT 3Ch also needs an encoded ESC and ModR/M. */
    if (ip < 2 || (following > 1 && ip > 0xffffffffu - (following - 1)) ||
        (!ldt_is_system(sel) && ip - 1 + following > ldt_get_limit(sel)))
    {
        WARN("x87 INT %02x extends beyond code segment at %04x:%08x\n", vector, sel, ip);
        return NULL;
    }
    code = ldt_get_ptr(sel, ip - 2);
    if (!code || code[0] != 0xcd || code[1] != vector)
    {
        WARN("x87 INT %02x call site does not contain CD %02x\n", vector, vector);
        return NULL;
    }
    return code;
}


/**********************************************************************
 *          DOSVM_Int34Handler
 *
 * Handler for int 34 (FLOATING POINT EMULATION - Opcode 0xd8).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int34Handler(I386_CONTEXT *context)
{
    TRACE("Int 0x34 called-- FP opcode 0xd8\n");
    FPU_ModifyCode(context, 0xd8);
}


/**********************************************************************
 *          DOSVM_Int35Handler
 *
 * Handler for int 35 (FLOATING POINT EMULATION - Opcode 0xd9).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int35Handler(I386_CONTEXT *context)
{
    TRACE("Int 0x35 called-- FP opcode 0xd9\n");
    FPU_ModifyCode(context, 0xd9);
}


/**********************************************************************
 *          DOSVM_Int36Handler
 *
 * Handler for int 36 (FLOATING POINT EMULATION - Opcode 0xda).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int36Handler(I386_CONTEXT *context)
{
    TRACE("Int 0x36 called-- FP opcode 0xda\n");
    FPU_ModifyCode(context, 0xda);
}


/**********************************************************************
 *          DOSVM_Int37Handler
 *
 * Handler for int 37 (FLOATING POINT EMULATION - Opcode 0xdb).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int37Handler(I386_CONTEXT *context)
{
    TRACE("Int 0x37 called-- FP opcode 0xdb\n");
    FPU_ModifyCode(context, 0xdb);
}


/**********************************************************************
 *          DOSVM_Int38Handler
 *
 * Handler for int 38 (FLOATING POINT EMULATION - Opcode 0xdc).
 *
 *  Between versions 3.0 and 5.01, the original PC-MOS API call that
 *  was here was moved to int 0xd4.
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int38Handler(I386_CONTEXT *context)
{
    TRACE("Int 0x38 called-- FP opcode 0xdc\n");
    FPU_ModifyCode(context, 0xdc);
}


/**********************************************************************
 *          DOSVM_Int39Handler
 *
 * Handler for int 39 (FLOATING POINT EMULATION - Opcode 0xdd).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int39Handler(I386_CONTEXT *context)
{
    TRACE("Int 0x39 called-- FP opcode 0xdd\n");
    FPU_ModifyCode(context, 0xdd);
}


/**********************************************************************
 *          DOSVM_Int3aHandler
 *
 * Handler for int 3a (FLOATING POINT EMULATION - Opcode 0xde).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int3aHandler(I386_CONTEXT *context)
{
    TRACE("Int 0x3a called-- FP opcode 0xde\n");
    FPU_ModifyCode(context, 0xde);
}


/**********************************************************************
 *          DOSVM_Int3bHandler
 *
 * Handler for int 3B (FLOATING POINT EMULATION - Opcode 0xdf).
 *
 *  The interrupt list isn't specific about what this interrupt
 *  actually does. [ interrup.m ]
 */
void WINAPI DOSVM_Int3bHandler(I386_CONTEXT *context)
{
    TRACE("Int 0x3b called-- FP opcode 0xdf\n");
    FPU_ModifyCode(context, 0xdf);
}


/**********************************************************************
 *          DOSVM_Int3cHandler
 *
 * Microsoft 8087 emulation rewrites WAIT + segment override + ESC as
 * CD 3C xx, where the upper two bits of xx identify the segment and
 * the low three bits retain the original D8h-DFh ESC opcode.
 *   xx = 18h-1Fh (DS), 58h-5Fh (SS), 98h-9Fh (CS), D8h-DFh (ES).
 * Reconstruct the original three-byte prefix in place and retry it.
 */
void WINAPI DOSVM_Int3cHandler(I386_CONTEXT *context)
{
    static const BYTE prefixes[] = { 0x3e, 0x36, 0x2e, 0x26 };
    BYTE *code = FPU_GetPatchSite(context, 0x3c, 2);
    BYTE encoded;

    if (!code) return;
    encoded = code[2];
    if ((encoded & 0x38) != 0x18)
    {
        WARN("x87 INT 3C contains an invalid encoded ESC opcode %02x\n", encoded);
        return;
    }
    code[0] = 0x9b;                       /* FWAIT */
    code[1] = prefixes[encoded >> 6];     /* DS, SS, CS, ES */
    code[2] = 0xd8 | (encoded & 7);       /* x87 ESC opcode */
    context->Eip -= 2;
    TRACE("Restored x87 segment override %02x ESC %02x\n", code[1], code[2]);
}


/**********************************************************************
 *          DOSVM_Int3dHandler
 *
 * Handler for int 3D (FLOATING POINT EMULATION - Standalone FWAIT).
 *
 *  Opcode 0x90 is a NOP.  It just fills space where the 3D was.
 */
void WINAPI DOSVM_Int3dHandler(I386_CONTEXT *context)
{
    TRACE("Int 0x3d called-- Standalone FWAIT\n");
    FPU_ModifyCode(context, 0x90);
}


/**********************************************************************
 *          DOSVM_Int3eHandler
 *
 * FLOATING POINT EMULATION -- Borland "Shortcut" call.
 *  The two bytes following the int 3E instruction are
 *  the subcode and a NOP ( 0x90 ), except for subcodes DC and DE
 *  where the second byte is the register count.
 *
 *  Direct access 4.0 modifies and does not restore this vector.
 *
 */
void WINAPI DOSVM_Int3eHandler(I386_CONTEXT *context)
{
    FIXME("Int 3E NOT Implemented\n");
    INT_BARF(context, 0x3e);
}


/**********************************************************************
 *          FPU_ModifyCode
 *
 *   This is the function that inserts the 0x9b fwait instruction
 *   and the actual FPU opcode into the program.
 *           -A.C.
 *
 *               Code thanks to Ove Kaaven
 */
static void FPU_ModifyCode(I386_CONTEXT *context, BYTE Opcode)
{
    BYTE vector = Opcode == 0x90 ? 0x3d : Opcode - 0xa4;
    BYTE *code = FPU_GetPatchSite(context, vector, 0);

    if (!code) return;
    code[0] = 0x9b;          /* FWAIT */
    code[1] = Opcode;        /* ESC D8h-DFh, or NOP after standalone FWAIT */
    context->Eip -= 2;      /* retry the restored instruction */

    TRACE("Restored x87 instruction 9B %02x from INT %02x\n", Opcode, vector);
}
