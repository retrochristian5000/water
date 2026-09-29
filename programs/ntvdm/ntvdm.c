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
#include "dosvm.h"
#include "sb20.h"
#include "wow.h"
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
    struct ntvdm_sb20_config sb20;
    const char *blaster = getenv( "BLASTER" );
    int i;
    NTSTATUS ret = STATUS_OBJECT_NAME_NOT_FOUND;
    DWORD written, drives = GetLogicalDrives(), keyb_codepage;
    BOOL have_keyb = get_dos_keyboard( keyb_layout, &keyb_codepage );
    BOOL valid_sb20 = ntvdm_sb20_configure( &sb20, blaster );

    if (!valid_sb20)
        WINE_WARN( "malformed BLASTER setting '%s'; disabling XP NTVDM SB2 compatibility\n",
                   blaster ? blaster : "" );
    else if (!sb20.enabled && sb20.base && sb20.card_type != NTVDM_SB20_CARD_TYPE)
        WINE_WARN( "BLASTER T%u requests a card unsupported by XP NTVDM; disabling SB emulation\n",
                   (unsigned int)sb20.card_type );

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
                        sizeof("[sblaster]\nsbtype=sb2\nsbbase=ffff\nirq=15\ndma=7\noplmode=opl2\n\n") +
                        sizeof("set BLASTER=AFFFF I15 D7 PFFFF T3\n") +
                        6 + strlen( app ) + strlen( args ) + 20 );
    if (!buffer)
    {
        CloseHandle( file );
        DeleteFileW( config );
        return 1;
    }

    p = buffer;
    p += sprintf( p, "[sblaster]\n" );
    if (sb20.enabled)
    {
        p += sprintf( p, "sbtype=sb2\nsbbase=%x\nirq=%u\ndma=%u\noplmode=opl2\n\n",
                      (unsigned int)sb20.base, (unsigned int)sb20.irq,
                      (unsigned int)sb20.dma );
        WINE_TRACE( "XP NTVDM SB2 profile A%x I%u D%u P%x T%u\n",
                    (unsigned int)sb20.base, (unsigned int)sb20.irq,
                    (unsigned int)sb20.dma, (unsigned int)sb20.mpu_base,
                    (unsigned int)sb20.card_type );
    }
    else
    {
        p += sprintf( p, "sbtype=none\noplmode=none\n\n" );
        WINE_TRACE( "XP NTVDM SB2 profile disabled\n" );
    }

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

    if (sb20.enabled)
        p += sprintf( p, "\nset BLASTER=A%X I%u D%u P%X T%u\n",
                      (unsigned int)sb20.base, (unsigned int)sb20.irq,
                      (unsigned int)sb20.dma, (unsigned int)sb20.mpu_base,
                      (unsigned int)sb20.card_type );
    else
        p += sprintf( p, "\nset BLASTER=A0\n" );

    p += sprintf( p, "config -securemode\n" );
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
    WINE_MESSAGE( "Usage: ntvdm.exe --app-name app.com [arguments]\n"
                  "       ntvdm.exe --wow-app-name app.exe command-line\n"
                  "       ntvdm.exe --prepare-only app.com [arguments]\n" );
}

int main( int argc, char **argv )
{
    struct dos_process process;
    enum dos_image_kind kind;
    const char *appname;
    char **app_args;
    char *args;
    BOOL prepare_only = FALSE, wow_app = FALSE;
    int ret;

    if (argc >= 3 && !strcmp( argv[1], "--app-name" ))
    {
        appname = argv[2];
        app_args = argv + 3;
    }
    else if (argc >= 3 && !strcmp( argv[1], "--wow-app-name" ))
    {
        wow_app = TRUE;
        appname = argv[2];
        app_args = argv + 3;
    }
    else if (argc >= 3 && !strcmp( argv[1], "--prepare-only" ))
    {
        prepare_only = TRUE;
        appname = argv[2];
        app_args = argv + 3;
    }
    else
    {
        usage();
        return 1;
    }

    if (wow_app)
    {
        /*
         * KernelBase passes the original Win16 command line after --wow-app-name;
         * like winevdm, skip its leading application-name token before building
         * the Pascal-style LoadModule16 command tail.
         */
        if (*app_args) app_args++;
        WINE_TRACE( "Win16 application = %s\n", appname );
        return wow_run_app( appname, app_args );
    }

    WINE_TRACE( "DOS application = %s\n", appname );

    if (!(args = build_dos_args( app_args ))) return 1;

    kind = dos_prepare_process( appname, args, &process );
    if (kind == DOS_IMAGE_COM)
    {
        if (prepare_only)
        {
            dos_release_process( &process );
            HeapFree( GetProcessHeap(), 0, args );
            return 0;
        }
        dos_release_process( &process );
    }
    else if (kind == DOS_IMAGE_MZ)
    {
        WINE_TRACE( "MZ image recognized; internal EXE relocation loader is not installed yet\n" );
        if (prepare_only)
        {
            HeapFree( GetProcessHeap(), 0, args );
            return 2;
        }
    }
    else
    {
        WINE_WARN( "unable to prepare internal DOS process image for %s (error %lu)\n",
                   appname, GetLastError() );
        if (prepare_only)
        {
            HeapFree( GetProcessHeap(), 0, args );
            return 1;
        }
    }

    /*
     * This is the intentional replacement seam.  COM files now have a real
     * Water PSP/memory/register image before execution falls back.  As CPU,
     * BIOS and INT 21h support arrives, execute that prepared process here
     * and shrink this fallback rather than moving DOS logic back to winevdm.
     */
    ret = run_dosbox( appname, args );

    HeapFree( GetProcessHeap(), 0, args );
    return ret;
}
