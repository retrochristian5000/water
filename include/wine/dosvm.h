/*
 * Shared DOS guest ABI definitions for Water VDM personalities.
 *
 * These definitions describe memory and register state visible to DOS code.
 * They do not describe NTVDM or WIN386 host/process state.
 */
#ifndef __WINE_DOSVM_H
#define __WINE_DOSVM_H

#include "windef.h"

#define WINE_DOS_CONVENTIONAL_MEMORY_SIZE 0x000a0000
#define WINE_DOS_PSP_SIZE                 0x0100
#define WINE_DOS_PSP_SEGMENT              0x1000
#define WINE_DOS_ENV_SEGMENT              0x0f00
#define WINE_DOS_COM_ENTRY_OFFSET         0x0100
#define WINE_DOS_COM_MAX_IMAGE_SIZE       0xff00
#define WINE_DOS_COMMAND_TAIL_MAX         126

#define WINE_DOS_PSP_FCB1_OFFSET          0x5c
#define WINE_DOS_PSP_FCB2_OFFSET          0x6c
#define WINE_DOS_PSP_COMMAND_TAIL_OFFSET  0x80

#pragma pack(push,1)
struct wine_dos_psp
{
    BYTE int20[2];             /* 00: INT 20h terminate stub */
    WORD memory_end;           /* 02: first segment beyond allocation */
    BYTE reserved04;           /* 04 */
    BYTE cpm_call;             /* 05: far CALL opcode */
    DWORD cpm_entry;           /* 06: CP/M compatibility entry */
    DWORD int22;               /* 0a: terminate vector */
    DWORD int23;               /* 0e: Ctrl-C vector */
    DWORD int24;               /* 12: critical-error vector */
    WORD parent_psp;           /* 16 */
    BYTE handles[20];          /* 18: initial job file table */
    WORD environment_segment;  /* 2c */
    DWORD saved_stack;         /* 2e */
    WORD max_handles;          /* 32 */
    DWORD handle_table;        /* 34: far pointer to handles[] */
    DWORD previous_psp;        /* 38 */
    BYTE reserved3c[4];        /* 3c */
    WORD dos_version;          /* 40: SETVER override / DOS version */
    WORD next_psp;             /* 42: Windows/DOS chaining field */
    BYTE reserved44[12];       /* 44 */
    BYTE int21_retf[3];        /* 50: INT 21h; RETF */
    BYTE reserved53[9];        /* 53 */
    BYTE data[WINE_DOS_PSP_SIZE - 0x5c];
};
#pragma pack(pop)

#define WINE_DOS_CPU_CONTEXT_SIZE 28

#pragma pack(push,2)
struct wine_dos_cpu_context
{
    WORD ax, bx, cx, dx;
    WORD si, di, bp, sp;
    WORD ip, flags;
    WORD cs, ds, es, ss;
};
#pragma pack(pop)

#endif /* __WINE_DOSVM_H */
