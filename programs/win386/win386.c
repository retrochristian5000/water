/*
 * Windows/386 enhanced-mode compatibility host
 *
 * WIN386.EXE is the DOS-based Windows 3.x enhanced-mode host personality.
 * It owns VMM/VM session identity and launches the System VM.  It is kept
 * deliberately separate from NTVDM, whose semantics belong to Windows NT.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/win386.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(win386);

static char *append_quoted_arg(char *dst, const char *src)
{
    unsigned int backslashes = 0;

    *dst++ = '"';
    while (*src)
    {
        if (*src == '\\')
        {
            *dst++ = *src++;
            backslashes++;
            continue;
        }
        if (*src == '"')
        {
            while (backslashes)
            {
                *dst++ = '\\';
                backslashes--;
            }
            *dst++ = '\\';
            *dst++ = *src++;
            backslashes = 0;
            continue;
        }
        backslashes = 0;
        *dst++ = *src++;
    }

    while (backslashes)
    {
        *dst++ = '\\';
        backslashes--;
    }
    *dst++ = '"';
    return dst;
}

static char *build_command_line(char **argv)
{
    SIZE_T size = 1;
    char *buffer, *p;
    int i;

    for (i = 0; argv[i]; i++) size += 2 * strlen(argv[i]) + 4;
    if (!(buffer = HeapAlloc(GetProcessHeap(), 0, size))) return NULL;

    p = buffer;
    for (i = 0; argv[i]; i++)
    {
        if (i) *p++ = ' ';
        p = append_quoted_arg(p, argv[i]);
    }
    *p = 0;
    return buffer;
}

static BOOL parse_version(const char *str, WORD *version)
{
    if (!strcmp(str, "3.0") || !strcmp(str, "3.00"))
    {
        *version = WATER_WIN386_VERSION_30;
        return TRUE;
    }
    if (!strcmp(str, "3.1") || !strcmp(str, "3.10"))
    {
        *version = WATER_WIN386_VERSION_31;
        return TRUE;
    }
    return FALSE;
}

static BOOL query_session(struct water_win386_session *copy)
{
    struct water_win386_session *state;
    char name[64];
    HANDLE mapping;
    DWORD len;

    len = GetEnvironmentVariableA(WATER_WIN386_SESSION_ENV, name, ARRAY_SIZE(name));
    if (!len || len >= ARRAY_SIZE(name)) return FALSE;

    mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, name);
    if (!mapping) return FALSE;

    state = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, sizeof(*state));
    if (!state)
    {
        CloseHandle(mapping);
        return FALSE;
    }

    if (state->magic == WATER_WIN386_MAGIC &&
        state->abi_version == WATER_WIN386_ABI_VERSION &&
        (state->flags & WATER_WIN386_FLAG_ACTIVE))
    {
        *copy = *state;
        UnmapViewOfFile(state);
        CloseHandle(mapping);
        return TRUE;
    }

    UnmapViewOfFile(state);
    CloseHandle(mapping);
    return FALSE;
}

static int show_status(void)
{
    struct water_win386_session state;
    char vm_text[16];
    DWORD len;

    if (!query_session(&state))
    {
        printf("WIN386 enhanced-mode session: inactive\n");
        return 1;
    }

    printf("WIN386 enhanced-mode session: active\n");
    printf("Owner PID: %lu\n", state.owner_pid);
    printf("Windows mux version: %u.%02u\n",
           LOBYTE(state.windows_mux_version), HIBYTE(state.windows_mux_version));
    printf("System VM: %u\n", state.system_vm);
    printf("Active VMs: %ld\n", state.active_vms);
    printf("Next VM: %ld\n", state.next_vm);

    len = GetEnvironmentVariableA(WATER_WIN386_VM_ENV, vm_text, ARRAY_SIZE(vm_text));
    if (len && len < ARRAY_SIZE(vm_text)) printf("Current VM: %s\n", vm_text);
    return 0;
}

static int run_system_vm(WORD version, char **argv)
{
    struct water_win386_session *state;
    PROCESS_INFORMATION process;
    STARTUPINFOA startup;
    char mapping_name[64], vm_text[16], *command;
    HANDLE mapping;
    DWORD exit_code = 1;

    if (!argv[0]) return 1;
    if (!(command = build_command_line(argv))) return 1;

    sprintf(mapping_name, "Water.Win386.%08lx", GetCurrentProcessId());

    mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                 sizeof(*state), mapping_name);
    if (!mapping)
    {
        HeapFree(GetProcessHeap(), 0, command);
        return 1;
    }

    state = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(*state));
    if (!state)
    {
        CloseHandle(mapping);
        HeapFree(GetProcessHeap(), 0, command);
        return 1;
    }

    memset(state, 0, sizeof(*state));
    state->magic = WATER_WIN386_MAGIC;
    state->abi_version = WATER_WIN386_ABI_VERSION;
    state->flags = WATER_WIN386_FLAG_ACTIVE | WATER_WIN386_FLAG_VMM;
    state->owner_pid = GetCurrentProcessId();
    state->windows_mux_version = version;
    state->system_vm = WATER_WIN386_VM_SYSTEM;
    state->next_vm = WATER_WIN386_VM_SYSTEM + 1;
    state->active_vms = 1;

    sprintf(vm_text, "%u", WATER_WIN386_VM_SYSTEM);
    SetEnvironmentVariableA(WATER_WIN386_SESSION_ENV, mapping_name);
    SetEnvironmentVariableA(WATER_WIN386_VM_ENV, vm_text);

    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    memset(&process, 0, sizeof(process));

    TRACE("starting System VM %u, version %u.%02u: %s\n",
          state->system_vm, LOBYTE(version), HIBYTE(version), debugstr_a(command));

    if (CreateProcessA(NULL, command, NULL, NULL, TRUE, 0, NULL, NULL, &startup, &process))
    {
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &exit_code);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    else
        ERR("unable to start System VM command %s, error %lu\n",
            debugstr_a(command), GetLastError());

    state->flags &= ~WATER_WIN386_FLAG_ACTIVE;
    state->active_vms = 0;

    SetEnvironmentVariableA(WATER_WIN386_VM_ENV, NULL);
    SetEnvironmentVariableA(WATER_WIN386_SESSION_ENV, NULL);

    UnmapViewOfFile(state);
    CloseHandle(mapping);
    HeapFree(GetProcessHeap(), 0, command);
    return exit_code;
}

static void usage(void)
{
    printf("Water Windows/386 enhanced-mode host\n\n"
           "win386.exe --system-vm [--version 3.0|3.1] command [args...]\n"
           "win386.exe --status\n");
}

int main(int argc, char **argv)
{
    WORD version = WATER_WIN386_VERSION_30;
    int arg = 1;

    if (argc == 2 && !strcmp(argv[1], "--status")) return show_status();

    if (argc < 3 || strcmp(argv[arg++], "--system-vm"))
    {
        usage();
        return 1;
    }

    if (arg < argc && !strcmp(argv[arg], "--version"))
    {
        if (arg + 1 >= argc || !parse_version(argv[arg + 1], &version))
        {
            fprintf(stderr, "win386: unsupported enhanced-mode version\n");
            return 1;
        }
        arg += 2;
    }

    if (arg >= argc)
    {
        usage();
        return 1;
    }

    return run_system_vm(version, argv + arg);
}
