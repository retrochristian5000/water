/*
 * Water NTVDM DOS process structures.
 *
 * These layouts describe guest DOS memory, not host ABI structures.
 */
#ifndef __WATER_NTVDM_DOSVM_H
#define __WATER_NTVDM_DOSVM_H

#include "windef.h"

#define DOS_CONVENTIONAL_MEMORY_SIZE 0x000a0000
#define DOS_PSP_SIZE                 0x0100
#define DOS_PSP_SEGMENT              0x1000
#define DOS_ENV_SEGMENT              0x0f00
#define DOS_COM_ENTRY_OFFSET         0x0100
#define DOS_COM_MAX_IMAGE_SIZE       0xff00
#define DOS_COMMAND_TAIL_MAX         126

#define DOS_PSP_FCB1_OFFSET          0x5c
#define DOS_PSP_FCB2_OFFSET          0x6c
#define DOS_PSP_COMMAND_TAIL_OFFSET  0x80

#pragma pack(push,1)
struct dos_psp
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
    BYTE data[DOS_PSP_SIZE - 0x5c];
};
#pragma pack(pop)

struct dos_cpu_context
{
    WORD ax, bx, cx, dx;
    WORD si, di, bp, sp;
    WORD ip, flags;
    WORD cs, ds, es, ss;
};

enum dos_image_kind
{
    DOS_IMAGE_INVALID,
    DOS_IMAGE_COM,
    DOS_IMAGE_MZ
};

enum dos_interrupt_result
{
    DOS_INTERRUPT_UNHANDLED,
    DOS_INTERRUPT_CONTINUE,
    DOS_INTERRUPT_TERMINATE
};

struct dos_process
{
    BYTE *memory;
    SIZE_T memory_size;
    WORD psp_segment;
    WORD environment_segment;
    DWORD image_size;
    DWORD dta;
    enum dos_image_kind image_kind;
    struct dos_cpu_context cpu;
    BYTE exit_code;
    BOOL terminated;
};

enum dos_image_kind dos_prepare_process(const char *path, const char *args,
                                         struct dos_process *process);
enum dos_interrupt_result dos_handle_interrupt(struct dos_process *process, BYTE vector);
void dos_release_process(struct dos_process *process);

#endif /* __WATER_NTVDM_DOSVM_H */
