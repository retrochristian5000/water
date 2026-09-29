/*
 * DOS .COM image loading for Water NTVDM.
 *
 * COM images are flat guest binaries.  Keep their loader separate from the
 * DOS-kernel/INT 21h personality and from MZ relocation handling.
 */

#include "windef.h"
#include "winbase.h"

#include "com.h"
#include "dosvm.h"

BOOL dos_load_com(HANDLE file, struct dos_process *process)
{
    LARGE_INTEGER size;
    BYTE *image, *stack;
    DWORD image_size, read;

    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0) return FALSE;

    /*
     * NT DOS reads FF00h bytes as a size probe.  If that buffer fills, EXEC
     * reports insufficient memory instead of accepting the image.  Thus the
     * largest accepted COM file is FEFFh bytes.
     */
    if (size.QuadPart > WINE_DOS_COM_MAX_IMAGE_SIZE)
    {
        SetLastError(ERROR_NOT_ENOUGH_MEMORY);
        return FALSE;
    }

    image_size = (DWORD)size.QuadPart;
    image = dos_memory_ptr(process, WINE_DOS_PSP_SEGMENT,
                           WINE_DOS_COM_ENTRY_OFFSET, image_size);
    if (!image) return FALSE;

    if (image_size &&
        (!ReadFile(file, image, image_size, &read, NULL) || read != image_size))
        return FALSE;

    /*
     * A normal COM process shares one segment for PSP, code/data and stack.
     * NT DOS gives it PSP:0100 as the entry point and, when at least 64 KiB is
     * available, places a zero return word at PSP:FFFE.
     */
    stack = dos_memory_ptr(process, WINE_DOS_PSP_SEGMENT,
                           WINE_DOS_COM_STACK_OFFSET, sizeof(WORD));
    if (!stack) return FALSE;
    stack[0] = 0;
    stack[1] = 0;

    process->image_size = image_size;
    process->cpu.cs = WINE_DOS_PSP_SEGMENT;
    process->cpu.ds = WINE_DOS_PSP_SEGMENT;
    process->cpu.es = WINE_DOS_PSP_SEGMENT;
    process->cpu.ss = WINE_DOS_PSP_SEGMENT;
    process->cpu.ip = WINE_DOS_COM_ENTRY_OFFSET;
    process->cpu.sp = WINE_DOS_COM_STACK_OFFSET;
    process->cpu.flags = 0x0200;
    return TRUE;
}
