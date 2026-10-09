/*
 * W95INF32 - Windows 95/98 SETUPX compatibility shim.
 *
 * The 2004 Wine developer analysis identified four entry points and a
 * separate Win16/Win32 thunk-data export.  This independent implementation
 * reuses Water's existing SetupAPI instead of pretending SETUPX works on
 * every Windows personality.
 *
 * Historical observations:
 * https://list.winehq.org/hyperkitty/list/wine-devel@list.winehq.org/2004/4/
 * The GenInstall32@20 decoration conflicts with its four-argument
 * prototype in that early sketch.  Preserve five x86 stack arguments
 * until a native Win9x binary or caller trace resolves that discrepancy.
 *
 * This library is free software; you may redistribute and/or modify it
 * under the GNU Lesser General Public License, version 2.1 or later.
 */

#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "setupapi.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(w95inf32);

/* Legacy SETUPX GenInstall flags are NOT the SetupAPI SPINST bit values. */
#define GENINSTALL_DO_FILES      0x01
#define GENINSTALL_DO_INI        0x02
#define GENINSTALL_DO_REG        0x04
#define GENINSTALL_DO_INI2REG    0x08
#define GENINSTALL_DO_CFGAUTO    0x10
#define GENINSTALL_DO_LOGCONFIG  0x20
#define GENINSTALL_DO_REGSRCPATH 0x40
#define GENINSTALL_DO_PERUSER    0x80
#define GENINSTALL_IMPLEMENTED   (GENINSTALL_DO_FILES | GENINSTALL_DO_INI | \
                                  GENINSTALL_DO_REG | GENINSTALL_DO_INI2REG | \
                                  GENINSTALL_DO_LOGCONFIG)

BOOL WINAPI CtlSetLddPath32(DWORD id, LPCSTR directory)
{
    TRACE("id %#lx, directory %s\n", id, debugstr_a(directory));

    if (!directory || !*directory)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    /* Only user-assigned directory IDs have equivalent SetupAPI behavior.
     * SETUPX's system LDD table is independent; do not claim to update it. */
    if (id < DIRID_USER || id > 0xffff)
    {
        FIXME("SETUPX system LDD id %#lx is not supported by SetupAPI\n", id);
        SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
        return FALSE;
    }

    return SetupSetDirectoryIdA(NULL, id, directory);
}

/* A bounded one-pass substitution for strings stored in an INF [Strings]
 * section. Native SETUPX also supports LDID and nested substitutions.
 * Without a length argument, the legacy caller must supply a destination
 * buffer of at least MAX_INF_STRING_LENGTH bytes.
 */
DWORD WINAPI GenFormStrWithoutPlaceHolders32(LPSTR dst, LPCSTR src, LPCSTR filename)
{
    char expanded[MAX_INF_STRING_LENGTH], name[MAX_INF_STRING_LENGTH];
    char replacement[MAX_INF_STRING_LENGTH];
    const char *p, *end;
    HINF inf;
    INFCONTEXT context;
    DWORD err = ERROR_SUCCESS;
    size_t used = 0, len;

    TRACE("dst %p, src %s, filename %s\n", dst, debugstr_a(src), debugstr_a(filename));

    if (!dst || !src || !filename || !*filename)
        return ERROR_INVALID_PARAMETER;
    inf = SetupOpenInfFileA(filename, NULL, INF_STYLE_WIN4, NULL);
    if (inf == INVALID_HANDLE_VALUE)
        return GetLastError();

    for (p = src; *p; )
    {
        if (*p != '%')
        {
            if (used + 1 >= sizeof(expanded))
            {
                err = ERROR_INSUFFICIENT_BUFFER;
                break;
            }
            expanded[used++] = *p++;
            continue;
        }

        if (p[1] == '%')
        {
            if (used + 1 >= sizeof(expanded))
            {
                err = ERROR_INSUFFICIENT_BUFFER;
                break;
            }
            expanded[used++] = '%';
            p += 2;
            continue;
        }

        end = strchr(p + 1, '%');
        if (!end)
        {
            err = ERROR_INVALID_DATA;
            break;
        }

        len = end - (p + 1);
        if (!len || len >= sizeof(name))
        {
            err = ERROR_INVALID_DATA;
            break;
        }
        memcpy(name, p + 1, len);
        name[len] = 0;
        if (!SetupFindFirstLineA(inf, "Strings", name, &context) ||
            !SetupGetStringFieldA(&context, 1, replacement, sizeof(replacement), NULL))
        {
            FIXME("unsupported or unknown INF replacement %s\n", debugstr_a(name));
            err = ERROR_CALL_NOT_IMPLEMENTED;
            break;
        }

        len = strlen(replacement);
        if (len >= sizeof(expanded) - used)
        {
            err = ERROR_INSUFFICIENT_BUFFER;
            break;
        }
        memcpy(expanded + used, replacement, len);
        used += len;
        p = end + 1;
    }

    if (!err)
    {
        expanded[used] = 0;
        memcpy(dst, expanded, used + 1);
    }
    SetupCloseInfFile(inf);
    return err;
}

DWORD WINAPI GenInstall32(LPCSTR filename, LPCSTR section, LPCSTR directory,
                          DWORD genflags, DWORD unverified_fifth)
{
    DWORD flags = 0, ret = ERROR_SUCCESS;
    HINF inf;
    void *context;
    BOOL installed;

    TRACE("filename %s, section %s, directory %s, flags %#lx, fifth %#lx\n",
          debugstr_a(filename), debugstr_a(section), debugstr_a(directory),
          genflags, unverified_fifth);

    if (!filename || !*filename || !section || !*section)
        return ERROR_INVALID_PARAMETER;

    /* The fifth parameter is present to match @20; don't ignore data that
     * the original API might actually use. */
    if (unverified_fifth || (genflags & ~GENINSTALL_IMPLEMENTED))
    {
        FIXME("unsupported SETUPX flags %#lx / extra argument %#lx\n",
              genflags & ~GENINSTALL_IMPLEMENTED, unverified_fifth);
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
    if (!genflags) return ERROR_INVALID_PARAMETER;

    if (genflags & GENINSTALL_DO_FILES) flags |= SPINST_FILES;
    if (genflags & GENINSTALL_DO_INI) flags |= SPINST_INIFILES;
    if (genflags & GENINSTALL_DO_REG) flags |= SPINST_REGISTRY;
    if (genflags & GENINSTALL_DO_INI2REG) flags |= SPINST_INI2REG;
    if (genflags & GENINSTALL_DO_LOGCONFIG) flags |= SPINST_LOGCONFIG;

    inf = SetupOpenInfFileA(filename, NULL, INF_STYLE_WIN4, NULL);
    if (inf == INVALID_HANDLE_VALUE) return GetLastError();

    context = SetupInitDefaultQueueCallback(NULL);
    if (!context)
    {
        ret = GetLastError();
        if (!ret) ret = ERROR_NOT_ENOUGH_MEMORY;
        SetupCloseInfFile(inf);
        return ret;
    }
    installed = SetupInstallFromInfSectionA(NULL, inf, section, flags, NULL,
                                             directory && *directory ? directory : NULL,
                                             SP_COPY_NEWER_OR_SAME,
                                             SetupDefaultQueueCallbackA,
                                             context, NULL, NULL);
    if (!installed)
    {
        ret = GetLastError();
        if (!ret) ret = ERROR_GEN_FAILURE;
    }

    SetupTermDefaultQueueCallback(context);
    SetupCloseInfFile(inf);
    return ret;
}

BOOL WINAPI GetSETUPXErrorText32(DWORD code, DWORD second, DWORD third)
{
    /* The historical signature records three DWORDs, without establishing
     * whether either trailing argument is a pointer or buffer length.
     * Guessing could overwrite caller memory. */
    FIXME("error %#lx, second %#lx, third %#lx: unknown SETUPX ABI\n",
          code, second, third);
    SetLastError(ERROR_CALL_NOT_IMPLEMENTED);
    return FALSE;
}
