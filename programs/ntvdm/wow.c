/*
 * NT WOW host for 16-bit Windows applications.
 *
 * Keep KRNL386 dynamically loaded here.  Native NT owns the WOW environment
 * before the 16-bit kernel begins executing; a static KRNL386 import would
 * invert that startup relationship.
 */

#include <stdlib.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "wine/vdm.h"
#include "wine/debug.h"

#include "wow.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntvdm);

typedef BOOL (WINAPI *w32_init_proc)(BOOL);
typedef HINSTANCE16 (WINAPI *load_library16_proc)(LPCSTR);
typedef HINSTANCE16 (WINAPI *load_module16_proc)(LPCSTR, LPVOID);
typedef SEGPTR (WINAPI *map_ls_proc)(void *);
typedef VOID (WINAPI *release_thunk_lock_proc)(DWORD *);
typedef VOID (WINAPI *restore_thunk_lock_proc)(DWORD);

struct wow_kernel_exports
{
    load_library16_proc load_library16;
    load_module16_proc load_module16;
    map_ls_proc map_ls;
    release_thunk_lock_proc release_thunk_lock;
    restore_thunk_lock_proc restore_thunk_lock;
};

static char *build_win16_command_line( char **argv )
{
    char **arg, *cmdline, *p;
    int len = 0;

    for (arg = argv; *arg; arg++)
    {
        const char *a = *arg;
        BOOL has_space = FALSE;
        int backslashes = 0;

        if (!*a) has_space = TRUE;
        while (*a)
        {
            if (*a == '\\') backslashes++;
            else
            {
                if (*a == ' ' || *a == '\t') has_space = TRUE;
                else if (*a == '"') len += 2 * backslashes + 1;
                backslashes = 0;
            }
            a++;
        }

        len += a - *arg + 1;
        if (has_space) len += 2;
    }

    if (!(cmdline = HeapAlloc( GetProcessHeap(), 0, len ? len + 1 : 2 )))
        return NULL;

    p = cmdline;
    *p++ = (len < 256) ? len : 0xff;

    for (arg = argv; *arg; arg++)
    {
        const char *a = *arg;
        BOOL has_space = FALSE, has_quote = FALSE;

        if (!*a) has_space = TRUE;
        while (*a)
        {
            if (*a == ' ' || *a == '\t') has_space = TRUE;
            else if (*a == '"') has_quote = TRUE;
            a++;
        }

        if (has_space) *p++ = '"';

        if (has_quote)
        {
            int backslashes = 0;
            a = *arg;
            while (*a)
            {
                if (*a == '\\')
                {
                    *p++ = *a++;
                    backslashes++;
                }
                else
                {
                    if (*a == '"')
                    {
                        int i;
                        for (i = 0; i <= backslashes; i++) *p++ = '\\';
                    }
                    *p++ = *a++;
                    backslashes = 0;
                }
            }
        }
        else
        {
            strcpy( p, *arg );
            p += strlen(*arg);
        }

        if (has_space) *p++ = '"';
        *p++ = ' ';
    }

    if (len) p--;
    *p = 0;
    return cmdline;
}

static BOOL load_wow_kernel( HMODULE kernel, struct wow_kernel_exports *exports )
{
    exports->load_library16 = (load_library16_proc)GetProcAddress( kernel, "LoadLibrary16" );
    exports->load_module16 = (load_module16_proc)GetProcAddress( kernel, "LoadModule16" );
    exports->map_ls = (map_ls_proc)GetProcAddress( kernel, "MapLS" );
    exports->release_thunk_lock =
        (release_thunk_lock_proc)GetProcAddress( kernel, "ReleaseThunkLock" );
    exports->restore_thunk_lock =
        (restore_thunk_lock_proc)GetProcAddress( kernel, "RestoreThunkLock" );

    return exports->load_library16 && exports->load_module16 && exports->map_ls &&
           exports->release_thunk_lock && exports->restore_thunk_lock;
}

int wow_run_app( const char *appname, char **argv )
{
    struct wow_kernel_exports kernel_exports;
    LOADPARAMS16 params;
    STARTUPINFOA startup;
    HINSTANCE16 instance;
    w32_init_proc w32_init;
    HMODULE wow32, kernel;
    DWORD lock_count;
    WORD show_cmd[2];
    char *cmdline;

    if (!SetEnvironmentVariableA( WATER_VDM_PERSONALITY_ENV, WATER_VDM_PERSONALITY_NT_WOW ))
    {
        ERR( "unable to mark NT WOW personality, error %lu\n", GetLastError() );
        return 1;
    }

    if (!(wow32 = LoadLibraryA( "wow32.dll" )) ||
        !(w32_init = (w32_init_proc)GetProcAddress( wow32, "W32Init" )) ||
        !w32_init( FALSE ))
    {
        ERR( "unable to initialize WOW32 before KRNL386\n" );
        return 1;
    }

    if (!(kernel = LoadLibraryA( "krnl386.exe16" )) ||
        !load_wow_kernel( kernel, &kernel_exports ))
    {
        ERR( "unable to load KRNL386 NT WOW entry points\n" );
        return 1;
    }

    if (!(cmdline = build_win16_command_line( argv ))) return 1;

    memset( &startup, 0, sizeof(startup) );
    startup.cb = sizeof(startup);
    GetStartupInfoA( &startup );

    show_cmd[0] = 2;
    show_cmd[1] = (startup.dwFlags & STARTF_USESHOWWINDOW) ? startup.wShowWindow : 1;

    params.hEnvironment = 0;
    params.cmdLine = kernel_exports.map_ls( cmdline );
    params.showCmd = kernel_exports.map_ls( show_cmd );
    params.reserved = 0;

    kernel_exports.restore_thunk_lock( 1 );

    /* Native NT WOW supplies the standard Win16 system DLL set in the VDM. */
    kernel_exports.load_library16( "gdi.exe" );
    kernel_exports.load_library16( "user.exe" );
    kernel_exports.load_library16( "mmsystem.dll" );

    instance = kernel_exports.load_module16( appname, &params );
    if (instance < 32)
    {
        kernel_exports.release_thunk_lock( &lock_count );
        ERR( "unable to start Win16 application %s, error %u\n",
             debugstr_a(appname), instance );
        HeapFree( GetProcessHeap(), 0, cmdline );
        return instance;
    }

    TRACE( "NT WOW task %04x started for %s\n", instance, debugstr_a(appname) );

    /*
     * The Win16 scheduler owns the process lifetime.  Match winevdm's existing
     * contract: the process exits when the last Win16 task tears it down.
     */
    kernel_exports.release_thunk_lock( &lock_count );
    HeapFree( GetProcessHeap(), 0, cmdline );
    Sleep( INFINITE );
    return 0;
}
