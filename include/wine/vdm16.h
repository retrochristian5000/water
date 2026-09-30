/*
 * Water NT WOW profile ABI for Win16 consumers.
 *
 * NTVDM selects the profile, WOW32 transports it as a process-local scalar,
 * and Win16 components such as KRNL386 consume it.  Keep these values out of
 * the public WOW thunk headers: they are Water-private NT WOW state.
 */
#ifndef __WINE_VDM16_H
#define __WINE_VDM16_H

#include "windef.h"

/*
 * Keep these as explicit 32-bit scalar values rather than a C enum so the
 * NTVDM -> WOW32 -> Win16 bridge is stable across host compilers and
 * architectures.
 */
#define WATER_VDM_WOW_PROFILE_NONE  0x00000000u
#define WATER_VDM_WOW_PROFILE_NT31  0x00000310u
#define WATER_VDM_WOW_PROFILE_NT351 0x00000351u
#define WATER_VDM_WOW_PROFILE_NT5   0x00000500u

/*
 * Native NT WOWINFO guest ABI used by KERNEL.WOWGetNextVDMCommand.
 * OpenNT builds this structure with 2-byte packing. Segmented pointers remain
 * explicit 32-bit values so the layout is independent of the Water host ABI.
 */
#pragma pack(push,2)
struct water_wowinfo16
{
    DWORD lp_cmd_line;
    DWORD lp_app_name;
    DWORD lp_environment;
    DWORD task_id;
    WORD cmd_line_size;
    WORD app_name_size;
    WORD environment_size;
    WORD current_drive;
    DWORD lp_current_directory;
    WORD current_directory_size;
    WORD show_window;
};
#pragma pack(pop)

/*
 * Process-local provider view. WOW32 translates segmented guest pointers
 * before calling NTVDM; these host pointers are never serialized.
 */
struct water_wow_command_buffers
{
    char *cmd_line;
    char *app_name;
    char *environment;
    char *current_directory;
    WORD cmd_line_size;
    WORD app_name_size;
    WORD environment_size;
    WORD current_directory_size;
    DWORD task_id;
    WORD current_drive;
    WORD show_window;
};

typedef BOOL (__cdecl *water_wow_next_command_proc)(struct water_wow_command_buffers *);

#endif /* __WINE_VDM16_H */
