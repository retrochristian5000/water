/*
 * NT Virtual DOS Machine compatibility host
 *
 * Water's DOS execution ownership lives here.  The initial backend remains
 * DOSBox while CPU, BIOS, DOS-kernel and VDD services are moved in-process.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "wine/doskeyb.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntvdm);

#define DOSBOX "dosbox"

static BOOL get_dos_keyboard( char layout[WINE_DOS_KEYB_MAX_LAYOUT + 1], DWORD *codepage )
{
    WCHAR layoutW[WINE_DOS_KEYB_MAX_LAYOUT + 1];
    DWORD type, size;
    HKEY key;
    unsigned int i;

    *codepage = 0;
    if (RegOpenKeyExW( HKEY_CURRENT_USER, WINE_DOS_KEYB_REGKEY, 0, KEY_QUERY_VALUE, &key ))
        return FALSE;

    size = sizeof(layoutW);
    if (RegQueryValueExW( key, WINE_DOS_KEYB_LAYOUT_VALUE, NULL, &type,
                          (BYTE *)layoutW, &size ) != ERROR_SUCCESS ||
        type != REG_SZ || !layoutW[0])
    {
        RegCloseKey( key );
        return FALSE;
    }
    layoutW[ARRAY_SIZE(layoutW) - 1] = 0;

    for (i = 0; layoutW[i]; i++)
    {
        if (i >= WINE_DOS_KEYB_MAX_LAYOUT ||
            !((layoutW[i] >= L'A' && layoutW[i] <= L'Z') ||
              (layoutW[i] >= L'a' && layoutW[i] <= L'z')))
        {
            RegCloseKey( key );
            return FALSE;
        }
        layout[i] = (char)layoutW[i];
    }
    if (i != 2)
    {
        RegCloseKey( key );
        return FALSE;
    }
    layout[i] = 0;

    size = sizeof(*codepage);
    if (RegQueryValueExW( key, WINE_DOS_KEYB_CODEPAGE_VALUE, NULL, &type,
                          (BYTE *)codepage, &size ) != ERROR_SUCCESS ||
        type != REG_DWORD || size != sizeof(*codepage) || !*codepage || *codepage > 65535)
        *codepage = 0;

    RegCloseKey( key );
    return TRUE;
}

static char *append_quoted_arg( char *dst, const char *src )
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

static char *build_dos_args( char **argv )
{
    SIZE_T size = 1;
    char *buffer, *p;
    int i;

    for (i = 0; argv[i]; i++) size += 2 * strlen(argv[i]) + 4;
    if (!(buffer = HeapAlloc( GetProcessHeap(), 0, size ))) return NULL;

    p = buffer;
    for (i = 0; argv[i]; i++)
    {
        if (i) *p++ = ' ';
        p = append_quoted_arg( p, argv[i] );
    }
    *p = 0;
    return buffer;
}

static int run_dosbox( const char *appname, const char *args )
{
    const WCHAR *config_dir = _wgetenv( L"WINECONFIGDIR" );
    WCHAR path[MAX_PATH], config[MAX_PATH];
    HANDLE file;
    char *p, *prefix, *buffer, app[MAX_PATH];
    char keyb_layout[WINE_DOS_KEYB_MAX_LAYOUT + 1];
    int i;
    NTSTATUS ret = STATUS_OBJECT_NAME_NOT_FOUND;
    DWORD written, drives = GetLogicalDrives(), keyb_codepage;
    BOOL have_keyb = get_dos_keyboard( keyb_layout, &keyb_codepage );

    if (!config_dir || !(prefix = wine_get_unix_file_name( config_dir ))) return 1;
    if (!GetTempPathW( MAX_PATH, path )) return 1;
    if (!GetTempFileNameW( path, L"cfg", 0, config )) return 1;
    if (!GetCurrentDirectoryW( MAX_PATH, path )) return 1;
    if (!GetShortPathNameA( appname, app, MAX_PATH )) return 1;
    GetShortPathNameW( path, path, MAX_PATH );

    file = CreateFileW( config, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, 0 );
    if (file == INVALID_HANDLE_VALUE) return 1;

    buffer = HeapAlloc( GetProcessHeap(), 0, sizeof("[autoexec]") +
                        sizeof("mount -z c") + sizeof("config -securemode") +
                        26 * (strlen(prefix) + sizeof("mount c /dosdevices/c:")) +
                        4 * lstrlenW( path ) +
                        sizeof("keyb ABCDE 65535") +
                        6 + strlen( app ) + strlen( args ) + 20 );
    if (!buffer)
    {
        CloseHandle( file );
        DeleteFileW( config );
        return 1;
    }

    p = buffer;
    p += sprintf( p, "[autoexec]\n" );
    for (i = 25; i >= 0; i--)
        if (!(drives & (1 << i)))
        {
            p += sprintf( p, "mount -z %c\n", 'a' + i );
            break;
        }

    for (i = 0; i <= 25; i++)
    {
        if (!(drives & (1 << i))) continue;
        p += sprintf( p, "mount %c %s/dosdevices/%c:\n", 'a' + i, prefix, 'a' + i );
    }

    p += sprintf( p, "%c:\ncd ", path[0] );
    p += WideCharToMultiByte( CP_UNIXCP, 0, path + 2, -1, p, 4 * lstrlenW(path), NULL, NULL ) - 1;

    if (have_keyb)
    {
        WINE_TRACE( "applying DOS keyboard layout %s, code page %lu\n",
                    keyb_layout, keyb_codepage );
        p += sprintf( p, "\nkeyb %s", keyb_layout );
        if (keyb_codepage) p += sprintf( p, " %lu", keyb_codepage );
    }

    p += sprintf( p, "\nconfig -securemode\n" );
    p += sprintf( p, "%s %s\n", app, args );
    p += sprintf( p, "exit\n" );

    if (WriteFile( file, buffer, strlen(buffer), &written, NULL ) && written == strlen(buffer))
    {
        const char *dosbox_argv[5];
        char *config_file = wine_get_unix_file_name( config );

        dosbox_argv[0] = DOSBOX;
        dosbox_argv[1] = "-userconf";
        dosbox_argv[2] = "-conf";
        dosbox_argv[3] = config_file;
        dosbox_argv[4] = NULL;
        ret = __wine_unix_spawnvp( (char **)dosbox_argv, TRUE );
    }

    CloseHandle( file );
    DeleteFileW( config );
    HeapFree( GetProcessHeap(), 0, buffer );

    if (FAILED(ret))
    {
        MESSAGE( "ntvdm: DOS backend unavailable while starting %s.\n", appname );
        return 1;
    }
    return 0;
}

static void usage(void)
{
    WINE_MESSAGE( "Usage: ntvdm.exe --app-name app.com [arguments]\n" );
}

int main( int argc, char **argv )
{
    char *args;
    int ret;

    if (argc < 3 || strcmp( argv[1], "--app-name" ))
    {
        usage();
        return 1;
    }

    WINE_TRACE( "DOS application = %s\n", argv[2] );

    if (!(args = build_dos_args( argv + 3 ))) return 1;

    /*
     * This is the intentional replacement seam.  As Water gains an internal
     * CPU/BIOS/DOS runtime, select it here and shrink the DOSBox fallback
     * rather than teaching winevdm more DOS-specific behavior.
     */
    ret = run_dosbox( argv[2], args );

    HeapFree( GetProcessHeap(), 0, args );
    return ret;
}
