/*
 * Windows NT DOS-kernel compatibility semantics for Water NTVDM.
 *
 * This is deliberately not an implementation of NTDOS.SYS as a host module.
 * The real NT design loads NTDOS.SYS as 16-bit guest code and uses DOS BOP/SVC
 * calls to reach the 32-bit DOS emulation manager.  These handlers are the
 * temporary semantic seam until Water can boot that guest/kernel path.
 */

#include "windef.h"

#include "doskrnl.h"

#define NTDOS_REPORTED_DOS_VERSION 0x0005 /* INT 21h AH=30h: DOS 5.00 */
#define NTDOS_TRUE_DOS_VERSION     0x3205 /* INT 21h AX=3306h: DOS 5.50 */

static void clear_carry(struct wine_dos_cpu_context *cpu)
{
    cpu->flags &= ~1;
}

static BYTE get_ah(const struct wine_dos_cpu_context *cpu)
{
    return cpu->ax >> 8;
}

static BYTE get_al(const struct wine_dos_cpu_context *cpu)
{
    return cpu->ax & 0xff;
}

static enum dos_interrupt_result handle_int21(struct dos_process *process)
{
    struct wine_dos_cpu_context *cpu = &process->cpu;
    BYTE function = get_ah(cpu);
    BYTE subfunction = get_al(cpu);

    switch (function)
    {
    case 0x00: /* terminate */
        process->exit_code = 0;
        process->terminated = TRUE;
        return DOS_INTERRUPT_TERMINATE;

    case 0x1a: /* set disk transfer area */
        process->dta = MAKELONG(cpu->dx, cpu->ds);
        clear_carry(cpu);
        return DOS_INTERRUPT_CONTINUE;

    case 0x2f: /* get disk transfer area */
        cpu->bx = LOWORD(process->dta);
        cpu->es = HIWORD(process->dta);
        clear_carry(cpu);
        return DOS_INTERRUPT_CONTINUE;

    case 0x30: /* get DOS version */
        /*
         * NT DOS reports 5.00 here.  BH is the OEM/version flag:
         * normal queries identify NT DOS with FFh, while AL=1 asks for
         * the DOS-5 version flag and receives zero for a non-ROM kernel.
         */
        cpu->ax = NTDOS_REPORTED_DOS_VERSION;
        cpu->bx = subfunction == 0x01 ? 0x0000 : 0xff00;
        cpu->cx = 0;
        clear_carry(cpu);
        return DOS_INTERRUPT_CONTINUE;

    case 0x33: /* extended DOS functions */
        if (subfunction == 0x06) /* get true DOS version */
        {
            cpu->bx = NTDOS_TRUE_DOS_VERSION;
            cpu->dx = 0; /* revision 0, unpatched DOS */
            clear_carry(cpu);
            return DOS_INTERRUPT_CONTINUE;
        }
        return DOS_INTERRUPT_UNHANDLED;

    case 0x4c: /* terminate with return code */
        process->exit_code = subfunction;
        process->terminated = TRUE;
        return DOS_INTERRUPT_TERMINATE;

    case 0x51: /* get current PSP */
    case 0x62: /* get PSP address */
        cpu->bx = process->psp_segment;
        clear_carry(cpu);
        return DOS_INTERRUPT_CONTINUE;

    default:
        return DOS_INTERRUPT_UNHANDLED;
    }
}

enum dos_interrupt_result dos_kernel_handle_interrupt(struct dos_process *process,
                                                       BYTE vector)
{
    switch (vector)
    {
    case 0x20:
        process->exit_code = 0;
        process->terminated = TRUE;
        return DOS_INTERRUPT_TERMINATE;

    case 0x21:
        return handle_int21(process);

    default:
        return DOS_INTERRUPT_UNHANDLED;
    }
}
