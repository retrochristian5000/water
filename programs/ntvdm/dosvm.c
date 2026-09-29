/*
 * DOS process image and PSP construction for Water NTVDM.
 *
 * This file intentionally builds the guest DOS ABI before CPU execution is
 * implemented in-process.  DOSBox remains an execution fallback in ntvdm.c.
 */

#include <stdio.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"

#include "dosvm.h"
#include "doskrnl.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntvdm);

C_ASSERT(sizeof(struct wine_dos_psp) == WINE_DOS_PSP_SIZE);
C_ASSERT(sizeof(struct wine_dos_cpu_context) == WINE_DOS_CPU_CONTEXT_SIZE);
C_ASSERT(FIELD_OFFSET(struct wine_dos_psp, memory_end) == 0x02);
C_ASSERT(FIELD_OFFSET(struct wine_dos_psp, environment_segment) == 0x2c);
C_ASSERT(FIELD_OFFSET(struct wine_dos_psp, max_handles) == 0x32);
C_ASSERT(FIELD_OFFSET(struct wine_dos_psp, int21_retf) == 0x50);
C_ASSERT(FIELD_OFFSET(struct wine_dos_psp, data) == WINE_DOS_PSP_FCB1_OFFSET);

static BYTE *dos_linear(struct dos_process *process, WORD segment, WORD offset, SIZE_T size)
{
    SIZE_T linear = ((SIZE_T)segment << 4) + offset;

    if (linear > process->memory_size || size > process->memory_size - linear)
        return NULL;
    return process->memory + linear;
}

static void init_default_fcb(BYTE *fcb)
{
    memset(fcb, 0, 16);
    memset(fcb + 1, ' ', 11);
}

static BOOL build_environment(struct dos_process *process, const char *path)
{
    BYTE *env;
    char short_path[MAX_PATH];
    SIZE_T path_len, capacity;
    DWORD short_len;
    WORD strings = 1;

    env = dos_linear(process, WINE_DOS_ENV_SEGMENT, 0, 0x1000);
    if (!env) return FALSE;

    short_len = GetShortPathNameA(path, short_path, ARRAY_SIZE(short_path));
    if (!short_len || short_len >= ARRAY_SIZE(short_path))
    {
        if (strlen(path) >= ARRAY_SIZE(short_path)) return FALSE;
        strcpy(short_path, path);
    }

    path_len = strlen(short_path) + 1;
    capacity = 0x1000;

    /* Empty DOS environment, followed by the DOS 3+ executable-name trailer. */
    if (2 + sizeof(strings) + path_len > capacity) return FALSE;

    env[0] = 0;
    env[1] = 0;
    memcpy(env + 2, &strings, sizeof(strings));
    memcpy(env + 2 + sizeof(strings), short_path, path_len);

    process->environment_segment = WINE_DOS_ENV_SEGMENT;
    return TRUE;
}

static BOOL build_psp(struct dos_process *process, const char *args)
{
    struct wine_dos_psp *psp;
    BYTE *fcb1, *fcb2, *tail;
    SIZE_T arg_len = strlen(args);
    SIZE_T tail_len = arg_len ? arg_len + 1 : 0;
    unsigned int i;

    if (tail_len > WINE_DOS_COMMAND_TAIL_MAX)
    {
        SetLastError(ERROR_BAD_LENGTH);
        return FALSE;
    }

    psp = (struct wine_dos_psp *)dos_linear(process, WINE_DOS_PSP_SEGMENT, 0, WINE_DOS_PSP_SIZE);
    if (!psp) return FALSE;

    memset(psp, 0, sizeof(*psp));

    psp->int20[0] = 0xcd;
    psp->int20[1] = 0x20;
    psp->memory_end = WINE_DOS_CONVENTIONAL_MEMORY_SIZE >> 4;
    psp->cpm_call = 0x9a;
    psp->cpm_entry = MAKELONG(0x00c0, 0x0000);

    /*
     * Water currently creates a top-level DOS process directly rather than
     * through COMMAND.COM, so use the PSP itself as the root parent.
     */
    psp->parent_psp = WINE_DOS_PSP_SEGMENT;

    memset(psp->handles, 0xff, sizeof(psp->handles));
    for (i = 0; i < 5; i++) psp->handles[i] = i;

    psp->environment_segment = process->environment_segment;
    psp->max_handles = ARRAY_SIZE(psp->handles);
    psp->handle_table = MAKELONG(FIELD_OFFSET(struct wine_dos_psp, handles), WINE_DOS_PSP_SEGMENT);
    psp->previous_psp = 0xffffffff;

    psp->int21_retf[0] = 0xcd;
    psp->int21_retf[1] = 0x21;
    psp->int21_retf[2] = 0xcb;

    fcb1 = (BYTE *)psp + WINE_DOS_PSP_FCB1_OFFSET;
    fcb2 = (BYTE *)psp + WINE_DOS_PSP_FCB2_OFFSET;
    init_default_fcb(fcb1);
    init_default_fcb(fcb2);

    tail = (BYTE *)psp + WINE_DOS_PSP_COMMAND_TAIL_OFFSET;
    tail[0] = tail_len;
    if (arg_len)
    {
        tail[1] = ' ';
        memcpy(tail + 2, args, arg_len);
    }
    tail[1 + tail_len] = 0x0d;

    process->psp_segment = WINE_DOS_PSP_SEGMENT;
    process->dta = MAKELONG(WINE_DOS_PSP_COMMAND_TAIL_OFFSET, WINE_DOS_PSP_SEGMENT);
    return TRUE;
}

static enum dos_image_kind detect_image_kind(HANDLE file)
{
    BYTE magic[2];
    DWORD read;

    if (!ReadFile(file, magic, sizeof(magic), &read, NULL) || read != sizeof(magic))
        return DOS_IMAGE_INVALID;

    SetFilePointer(file, 0, NULL, FILE_BEGIN);

    if ((magic[0] == 'M' && magic[1] == 'Z') ||
        (magic[0] == 'Z' && magic[1] == 'M'))
        return DOS_IMAGE_MZ;

    return DOS_IMAGE_COM;
}

static BOOL load_com(HANDLE file, struct dos_process *process)
{
    LARGE_INTEGER size;
    BYTE *image, *stack;
    DWORD image_size, read;

    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 ||
        size.QuadPart > WINE_DOS_COM_MAX_IMAGE_SIZE)
    {
        SetLastError(ERROR_BAD_FORMAT);
        return FALSE;
    }

    image_size = size.QuadPart;
    image = dos_linear(process, WINE_DOS_PSP_SEGMENT, WINE_DOS_COM_ENTRY_OFFSET, image_size);
    if (!image) return FALSE;

    if (image_size &&
        (!ReadFile(file, image, image_size, &read, NULL) || read != image_size))
        return FALSE;

    /*
     * DOS starts .COM files at PSP:0100 with all data segments at the PSP.
     * The zero word at FFFEh lets a plain RET reach PSP:0000 / INT 20h.
     */
    stack = dos_linear(process, WINE_DOS_PSP_SEGMENT, 0xfffe, sizeof(WORD));
    if (!stack) return FALSE;
    stack[0] = 0;
    stack[1] = 0;

    process->image_size = image_size;
    process->cpu.cs = WINE_DOS_PSP_SEGMENT;
    process->cpu.ds = WINE_DOS_PSP_SEGMENT;
    process->cpu.es = WINE_DOS_PSP_SEGMENT;
    process->cpu.ss = WINE_DOS_PSP_SEGMENT;
    process->cpu.ip = WINE_DOS_COM_ENTRY_OFFSET;
    process->cpu.sp = 0xfffe;
    process->cpu.flags = 0x0200;
    return TRUE;
}

enum dos_image_kind dos_prepare_process(const char *path, const char *args,
                                         struct dos_process *process)
{
    enum dos_image_kind kind;
    HANDLE file;

    memset(process, 0, sizeof(*process));

    file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                       FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return DOS_IMAGE_INVALID;

    kind = detect_image_kind(file);
    if (kind == DOS_IMAGE_INVALID)
    {
        CloseHandle(file);
        return kind;
    }

    /*
     * MZ recognition is deliberate: the PSP applies there too, but relocation
     * and EXE register setup belong to the next loader slice.  Do not pretend
     * an MZ image is a .COM file.
     */
    if (kind == DOS_IMAGE_MZ)
    {
        CloseHandle(file);
        return kind;
    }

    if (!(process->memory = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                      WINE_DOS_CONVENTIONAL_MEMORY_SIZE)))
    {
        CloseHandle(file);
        return DOS_IMAGE_INVALID;
    }
    process->memory_size = WINE_DOS_CONVENTIONAL_MEMORY_SIZE;
    process->image_kind = kind;

    if (!build_environment(process, path) ||
        !build_psp(process, args) ||
        !load_com(file, process))
    {
        CloseHandle(file);
        dos_release_process(process);
        return DOS_IMAGE_INVALID;
    }

    CloseHandle(file);

    TRACE("prepared COM PSP=%04x env=%04x DTA=%04x:%04x CS:IP=%04x:%04x SS:SP=%04x:%04x image=%lu\n",
          process->psp_segment, process->environment_segment,
          HIWORD(process->dta), LOWORD(process->dta),
          process->cpu.cs, process->cpu.ip, process->cpu.ss, process->cpu.sp,
          process->image_size);

    return kind;
}

enum dos_interrupt_result dos_handle_interrupt(struct dos_process *process, BYTE vector)
{
    if (!process || !process->memory) return DOS_INTERRUPT_UNHANDLED;
    return dos_kernel_handle_interrupt(process, vector);
}

void dos_release_process(struct dos_process *process)
{
    if (process->memory) HeapFree(GetProcessHeap(), 0, process->memory);
    memset(process, 0, sizeof(*process));
}
