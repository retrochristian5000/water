/*
 * Water NTVDM DOS process structures.
 *
 * These layouts describe guest DOS memory, not host ABI structures.
 */
#ifndef __WATER_NTVDM_DOSVM_H
#define __WATER_NTVDM_DOSVM_H

#include "windef.h"
#include "wine/dosvm.h"

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
    struct wine_dos_cpu_context cpu;
    BYTE exit_code;
    BOOL terminated;
};

enum dos_image_kind dos_prepare_process(const char *path, const char *args,
                                         struct dos_process *process);
enum dos_interrupt_result dos_handle_interrupt(struct dos_process *process, BYTE vector);
void dos_release_process(struct dos_process *process);

#endif /* __WATER_NTVDM_DOSVM_H */
