/*
 * Windows/386 enhanced-mode session bridge for KRNL386.
 *
 * This deliberately reads only the explicit WIN386 session ABI.  NTVDM does
 * not publish this state, which prevents the DOS-based Windows personality
 * from bleeding into Windows NT's VDM path.
 */

#include <stdlib.h>

#include "windef.h"
#include "winbase.h"
#include "kernel16_private.h"
#include "win386.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(vxd);

C_ASSERT(sizeof(struct water_win386_session) == WATER_WIN386_SESSION_SIZE);
C_ASSERT(FIELD_OFFSET(struct water_win386_session, windows_mux_version) == 16);
C_ASSERT(FIELD_OFFSET(struct water_win386_session, dos_version) == 20);
C_ASSERT(FIELD_OFFSET(struct water_win386_session, next_vm) == 24);

BOOL WIN386_QuerySession(struct win386_session_info *info)
{
    struct water_win386_session *state;
    char mapping_name[64], vm_text[16];
    HANDLE mapping;
    DWORD len;
    BOOL ret = FALSE;
    unsigned long vm = WATER_WIN386_VM_SYSTEM;

    len = GetEnvironmentVariableA(WATER_WIN386_SESSION_ENV, mapping_name, ARRAY_SIZE(mapping_name));
    if (!len || len >= ARRAY_SIZE(mapping_name)) return FALSE;

    mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, mapping_name);
    if (!mapping) return FALSE;

    state = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, WATER_WIN386_SESSION_SIZE);
    if (!state)
    {
        CloseHandle(mapping);
        return FALSE;
    }

    if (state->magic != WATER_WIN386_MAGIC ||
        state->abi_version != WATER_WIN386_ABI_VERSION ||
        !(state->flags & WATER_WIN386_FLAG_ACTIVE) ||
        !(state->flags & WATER_WIN386_FLAG_VMM) ||
        !state->system_vm || !state->dos_version)
        goto done;

    if (info)
    {
        info->windows_version = state->windows_mux_version;
        info->dos_version = state->dos_version;
        info->system_vm = state->system_vm;

        len = GetEnvironmentVariableA(WATER_WIN386_VM_ENV, vm_text, ARRAY_SIZE(vm_text));
        if (len && len < ARRAY_SIZE(vm_text))
        {
            char *end;
            unsigned long parsed = strtoul(vm_text, &end, 10);

            if (!*end && parsed >= state->system_vm &&
                parsed < (unsigned long)state->next_vm && parsed < 0x10000)
                vm = parsed;
        }
        info->current_vm = vm;
    }

    ret = TRUE;

done:
    UnmapViewOfFile(state);
    CloseHandle(mapping);
    return ret;
}
