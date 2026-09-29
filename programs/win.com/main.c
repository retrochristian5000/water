/*
 * Windows compatibility launcher
 *
 * Windows NT keeps a WIN.COM compatibility program for DOS-era installers
 * which invoke "win application".  Despite the .com suffix this program is a
 * normal Win32 PE image; executable format detection must win over filename
 * extension.
 */

#include <windows.h>

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
     * Preserve the historical WIN command shape.  Mode-selection switches
     * such as /R, /2, /3, /S and /N are accepted for compatibility but have
     * no meaning to the NT-style launcher.
     */
    while (*p == '/' || *p == '-')
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
