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
typedef void (__cdecl *w32_register_dos_int21_proc)(void *);
typedef HINSTANCE16 (WINAPI *load_library16_proc)(LPCSTR);
typedef HINSTANCE16 (WINAPI *load_module16_proc)(LPCSTR, LPVOID);
typedef SEGPTR (WINAPI *map_ls_proc)(void *);
typedef void *(WINAPI *map_sl_proc)(SEGPTR);
typedef VOID (WINAPI *release_thunk_lock_proc)(DWORD *);
typedef VOID (WINAPI *restore_thunk_lock_proc)(DWORD);

struct wow_kernel_exports
{
    load_library16_proc load_library16;
    load_module16_proc load_module16;
    map_ls_proc map_ls;
    map_sl_proc map_sl;
    release_thunk_lock_proc release_thunk_lock;
    restore_thunk_lock_proc restore_thunk_lock;
};

struct wow_dos_state
{
    BYTE current_drive;
    char directory[26][MAX_PATH];
};

static struct wow_dos_state wow_dos;
static map_sl_proc wow_map_sl;

static void set_reg_word( DWORD *reg, WORD value )
{
    *reg = (*reg & 0xffff0000u) | value;
}

static void set_reg_low_byte( DWORD *reg, BYTE value )
{
    *reg = (*reg & 0xffffff00u) | value;
}

static void set_reg_high_byte( DWORD *reg, BYTE value )
{
    *reg = (*reg & 0xffff00ffu) | ((DWORD)value << 8);
}

static void wow_dos_success( I386_CONTEXT *context )
{
    context->EFlags &= ~1u;
}

static void wow_dos_error( I386_CONTEXT *context, DWORD error )
{
    if (!error) error = ERROR_INVALID_FUNCTION;
    SetLastError( error );
    set_reg_word( &context->Eax, (WORD)error );
    context->EFlags |= 1;
}

static BOOL wow_dos_refresh_directory(void)
{
    char path[MAX_PATH];
    DWORD len;
    BYTE drive;

    len = GetCurrentDirectoryA( ARRAY_SIZE(path), path );
    if (!len || len >= ARRAY_SIZE(path) || path[1] != ':') return FALSE;

    if (path[0] >= 'a' && path[0] <= 'z') path[0] -= 'a' - 'A';
    if (path[0] < 'A' || path[0] > 'Z') return FALSE;

    drive = path[0] - 'A';
    wow_dos.current_drive = drive;
    lstrcpynA( wow_dos.directory[drive], path, ARRAY_SIZE(wow_dos.directory[drive]) );
    return TRUE;
}

static void wow_dos_init(void)
{
    memset( &wow_dos, 0, sizeof(wow_dos) );
    if (!wow_dos_refresh_directory())
    {
        wow_dos.current_drive = 2;  /* C: */
        lstrcpyA( wow_dos.directory[2], "C:\\" );
    }
}

static BOOL wow_dos_select_drive( I386_CONTEXT *context )
{
    BYTE drive = LOWORD(context->Edx) & 0xff;
    char root[] = "A:\\";
    const char *directory;

    if (drive < ARRAY_SIZE(wow_dos.directory))
    {
        root[0] += drive;
        if (GetDriveTypeA( root ) != DRIVE_NO_ROOT_DIR)
        {
            directory = wow_dos.directory[drive][0] ? wow_dos.directory[drive] : root;
            if (SetCurrentDirectoryA( directory ))
            {
                wow_dos.current_drive = drive;
                wow_dos_refresh_directory();
            }
        }
    }

    /* DOS AH=0Eh reports the logical-drive count in AL and has no CF error. */
    set_reg_low_byte( &context->Eax, ARRAY_SIZE(wow_dos.directory) );
    wow_dos_success( context );
    return TRUE;
}

static BOOL wow_dos_set_current_directory( I386_CONTEXT *context )
{
    const char *path;
    WCHAR pathW[MAX_PATH], fullW[MAX_PATH], env_name[4];
    DWORD attr;
    BYTE drive;

    if (!wow_map_sl ||
        !(path = wow_map_sl( MAKESEGPTR( (WORD)context->SegDs, LOWORD(context->Edx) ) )))
    {
        wow_dos_error( context, ERROR_INVALID_ADDRESS );
        return TRUE;
    }

    if (!MultiByteToWideChar( CP_OEMCP, 0, path, -1, pathW, ARRAY_SIZE(pathW) ) ||
        !GetFullPathNameW( pathW, ARRAY_SIZE(fullW), fullW, NULL ))
    {
        wow_dos_error( context, GetLastError() );
        return TRUE;
    }

    attr = GetFileAttributesW( fullW );
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY) ||
        fullW[1] != ':' || ((fullW[0] | 0x20) < 'a') || ((fullW[0] | 0x20) > 'z'))
    {
        wow_dos_error( context, ERROR_PATH_NOT_FOUND );
        return TRUE;
    }

    drive = (fullW[0] | 0x20) - 'a';
    WideCharToMultiByte( CP_OEMCP, 0, fullW, -1, wow_dos.directory[drive],
                         ARRAY_SIZE(wow_dos.directory[drive]), NULL, NULL );

    /*
     * DOS keeps a current directory per drive.  "=X:" is the Win32
     * representation of that state.  AH=3Bh must not select X: merely because
     * the supplied pathname names that drive.
     */
    env_name[0] = '=';
    env_name[1] = 'A' + drive;
    env_name[2] = ':';
    env_name[3] = 0;
    SetEnvironmentVariableW( env_name, fullW );

    if (drive == wow_dos.current_drive && !SetCurrentDirectoryW( fullW ))
    {
        wow_dos_error( context, GetLastError() );
        return TRUE;
    }

    wow_dos_success( context );
    return TRUE;
}

static BOOL wow_dos_get_current_directory( I386_CONTEXT *context )
{
    BYTE requested = LOWORD(context->Edx) & 0xff;
    BYTE drive = requested ? requested - 1 : wow_dos.current_drive;
    char root[] = "A:\\";
    const char *path, *relative;
    char *buffer;

    if (drive >= ARRAY_SIZE(wow_dos.directory) || !wow_map_sl ||
        !(buffer = wow_map_sl( MAKESEGPTR( (WORD)context->SegDs, LOWORD(context->Esi) ) )))
    {
        wow_dos_error( context, ERROR_INVALID_DRIVE );
        return TRUE;
    }

    if (!wow_dos.directory[drive][0])
    {
        root[0] += drive;
        if (GetDriveTypeA( root ) == DRIVE_NO_ROOT_DIR)
        {
            wow_dos_error( context, ERROR_INVALID_DRIVE );
            return TRUE;
        }
        lstrcpyA( wow_dos.directory[drive], root );
    }

    path = wow_dos.directory[drive];
    relative = path;
    if (path[1] == ':')
    {
        relative = path + 2;
        while (*relative == '\\' || *relative == '/') relative++;
    }

    /* DOS AH=47h specifies a 64-byte caller buffer. */
    lstrcpynA( buffer, relative, 64 );
    wow_dos_success( context );
    return TRUE;
}

static BOOL wow_dos_delete_file( I386_CONTEXT *context )
{
    const char *path;
    WCHAR pathW[MAX_PATH];

    if (!wow_map_sl ||
        !(path = wow_map_sl( MAKESEGPTR( (WORD)context->SegDs, LOWORD(context->Edx) ) )))
    {
        wow_dos_error( context, ERROR_INVALID_ADDRESS );
        return TRUE;
    }

    if (!MultiByteToWideChar( CP_OEMCP, 0, path, -1, pathW, ARRAY_SIZE(pathW) ) ||
        !DeleteFileW( pathW ))
    {
        wow_dos_error( context, GetLastError() );
        return TRUE;
    }

    wow_dos_success( context );
    return TRUE;
}

static BOOL wow_dos_file_attributes( I386_CONTEXT *context )
{
    const char *path;
    WCHAR pathW[MAX_PATH];
    DWORD attr;
    BYTE subfunction = LOWORD(context->Eax) & 0xff;

    if (subfunction > 1) return FALSE;

    if (!wow_map_sl ||
        !(path = wow_map_sl( MAKESEGPTR( (WORD)context->SegDs, LOWORD(context->Edx) ) )))
    {
        wow_dos_error( context, ERROR_INVALID_ADDRESS );
        return TRUE;
    }

    if (!MultiByteToWideChar( CP_OEMCP, 0, path, -1, pathW, ARRAY_SIZE(pathW) ))
    {
        wow_dos_error( context, GetLastError() );
        return TRUE;
    }

    if (!subfunction)
    {
        size_t len = lstrlenW( pathW );

        if (!len || pathW[len - 1] == '\\' || pathW[len - 1] == '/')
        {
            wow_dos_error( context, ERROR_FILE_NOT_FOUND );
            return TRUE;
        }

        attr = GetFileAttributesW( pathW );
        if (attr == INVALID_FILE_ATTRIBUTES)
        {
            wow_dos_error( context, GetLastError() );
            return TRUE;
        }

        set_reg_word( &context->Ecx, (WORD)attr );
    }
    else if (!SetFileAttributesW( pathW, LOWORD(context->Ecx) ))
    {
        wow_dos_error( context, GetLastError() );
        return TRUE;
    }

    wow_dos_success( context );
    return TRUE;
}

static BOOL wow_dos_ioctl( I386_CONTEXT *context )
{
    BYTE subfunction = LOWORD(context->Eax) & 0xff;
    BYTE drive = LOWORD(context->Ebx) & 0xff;
    char root[] = "A:\\";
    UINT type;

    /* NT5's WOW quick path only handles DOS IOCTL subfunction 08h. */
    if (subfunction != 0x08) return FALSE;

    if (!drive) drive = wow_dos.current_drive + 1;
    if (!drive || drive > ARRAY_SIZE(wow_dos.directory))
    {
        wow_dos_error( context, ERROR_INVALID_DRIVE );
        return TRUE;
    }

    root[0] += drive - 1;
    type = GetDriveTypeA( root );
    if (type == DRIVE_UNKNOWN || type == DRIVE_NO_ROOT_DIR)
    {
        wow_dos_error( context, ERROR_INVALID_DRIVE );
        return TRUE;
    }

    /* DOS returns AX=0 for removable media and AX=1 for non-removable. */
    set_reg_word( &context->Eax, type == DRIVE_REMOVABLE ? 0 : 1 );
    wow_dos_success( context );
    return TRUE;
}

static BOOL WINAPI wow_ntvdm_int21( I386_CONTEXT *context )
{
    SYSTEMTIME time;
    BYTE function;

    if (!context) return FALSE;
    function = (context->Eax >> 8) & 0xff;

    switch (function)
    {
    case 0x0e:  /* select default drive */
        return wow_dos_select_drive( context );

    case 0x19:  /* get default drive */
        set_reg_low_byte( &context->Eax, wow_dos.current_drive );
        wow_dos_success( context );
        return TRUE;

    case 0x2a:  /* get date */
        GetLocalTime( &time );
        set_reg_word( &context->Ecx, time.wYear );
        set_reg_high_byte( &context->Edx, time.wMonth );
        set_reg_low_byte( &context->Edx, time.wDay );
        set_reg_low_byte( &context->Eax, time.wDayOfWeek );
        wow_dos_success( context );
        return TRUE;

    case 0x3b:  /* set current directory */
        return wow_dos_set_current_directory( context );

    case 0x41:  /* delete file */
        return wow_dos_delete_file( context );

    case 0x43:  /* get/set file attributes */
        return wow_dos_file_attributes( context );

    case 0x44:  /* IOCTL: NT5 WOW quick path only handles AL=08h */
        return wow_dos_ioctl( context );

    case 0x47:  /* get current directory */
        return wow_dos_get_current_directory( context );

    default:
        return FALSE;
    }
}

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
    exports->map_sl = (map_sl_proc)GetProcAddress( kernel, "MapSL" );
    exports->release_thunk_lock =
        (release_thunk_lock_proc)GetProcAddress( kernel, "ReleaseThunkLock" );
    exports->restore_thunk_lock =
        (restore_thunk_lock_proc)GetProcAddress( kernel, "RestoreThunkLock" );

    return exports->load_library16 && exports->load_module16 && exports->map_ls &&
           exports->map_sl && exports->release_thunk_lock && exports->restore_thunk_lock;
}

int wow_run_app( const char *appname, char **argv )
{
    struct wow_kernel_exports kernel_exports;
    LOADPARAMS16 params;
    STARTUPINFOA startup;
    HINSTANCE16 instance;
    w32_init_proc w32_init;
    w32_register_dos_int21_proc register_dos_int21;
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

    register_dos_int21 =
        (w32_register_dos_int21_proc)GetProcAddress( wow32, "__wine_W32RegisterDosInt21Handler" );
    if (!register_dos_int21)
    {
        ERR( "WOW32 does not provide the NTVDM DOS service bridge\n" );
        return 1;
    }

    wow_map_sl = kernel_exports.map_sl;
    wow_dos_init();
    register_dos_int21( wow_ntvdm_int21 );

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
