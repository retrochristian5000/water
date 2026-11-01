/*
 * Windows compatibility launcher
 *
 * Windows NT keeps a WIN.COM compatibility program for DOS-era installers
 * which invoke "win application".  Despite the .com suffix this program is a
 * normal Win32 PE image; executable format detection must win over filename
 * extension.
 */

#include <windows.h>
#include <string.h>

static char *skip_blanks( char *p )
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static char *skip_word( char *p )
{
    while (*p && *p != ' ' && *p != '\t') p++;
    return p;
}

/* This NT compatibility launcher accepts the historical mode spellings,
 * but does not select DOS-Windows real, standard or enhanced mode.  Consume
 * only an exact one-letter switch; an unknown option is not disposable. */
static BOOL is_legacy_mode_switch( const char *p )
{
    char option;

    if ((*p != '/' && *p != '-') || !p[1] ||
        (p[2] && p[2] != ' ' && p[2] != '\t'))
        return FALSE;

    option = p[1];
    if (option >= 'a' && option <= 'z') option -= 'a' - 'A';
    return option == 'R' || option == 'S' || option == '2' || option == '3';
}

int WINAPI WinMain( HINSTANCE instance, HINSTANCE prev_instance, char *cmdline, int show )
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    DWORD exit_code;
    char *command, *p;
    SIZE_T size;

    (void)instance;
    (void)prev_instance;

    p = skip_blanks( cmdline );

    /*
     * This is an NT-style PE compatibility launcher, NOT the DOS-Windows
     * WIN.COM loader. Windows 3.0 used /R for real mode, /S (/2) for
     * standard mode and /3 for enhanced mode. Windows 3.1 removed real
     * mode; its WIN.COM checks CPU/XMS, selects WSWAP/DOSX/KRNL286 for
     * standard mode or WIN386/KRNL386 for 386 enhanced mode, and supports
     * /B boot logging and /D:FSVX troubleshooting. WfW 3.11 requires
     * enhanced mode and has a separate /N network-suppression option.
     *
     * This wrapper intentionally consumes only the historical one-letter
     * forms listed in is_legacy_mode_switch(), including /R as a legacy
     * compatibility spelling. It does not model any of these modes,
     * initialize SYSTEM.INI VxDs or select the SYSTEM.INI shell.
     * Keep that guest startup responsibility outside NT WIN.COM.
     */
    while (is_legacy_mode_switch( p ))
    {
        p = skip_blanks( skip_word( p ) );
    }

    if (!*p) return 0;

    size = strlen( p ) + 1;
    if (!(command = HeapAlloc( GetProcessHeap(), 0, size ))) return ERROR_NOT_ENOUGH_MEMORY;
    memcpy( command, p, size );

    ZeroMemory( &startup, sizeof(startup) );
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW | STARTF_FORCEONFEEDBACK;
    startup.wShowWindow = show;
    ZeroMemory( &process, sizeof(process) );

    if (!CreateProcessA( NULL, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process ))
    {
        exit_code = GetLastError();
        HeapFree( GetProcessHeap(), 0, command );
        return exit_code;
    }

    HeapFree( GetProcessHeap(), 0, command );
    CloseHandle( process.hThread );

    if (WaitForSingleObject( process.hProcess, INFINITE ) == WAIT_FAILED)
        exit_code = GetLastError();
    else if (!GetExitCodeProcess( process.hProcess, &exit_code ))
        exit_code = GetLastError();

    CloseHandle( process.hProcess );
    return exit_code;
}
