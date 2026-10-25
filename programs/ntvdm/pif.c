/*
 * Legacy DOS Program Information File (PIF) decoder for Water NTVDM.
 *
 * WineVDM previously interpreted PIF files before forwarding DOS programs
 * to NTVDM.  Keep the decoder with the DOS host and validate all file-
 * supplied offsets and strings rather than duplicating that process hop.
 */
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "pif.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ntvdm);

#define PIF_HEADER_SIZE       0x171
#define PIF_RECORD_SIZE      22
#define PIF_386_RECORD_SIZE 104

static WORD pif_word( const BYTE *data )
{
    return (WORD)data[0] | ((WORD)data[1] << 8);
}

static void pif_string( char *dst, SIZE_T dst_size, const BYTE *src, SIZE_T src_size )
{
    SIZE_T len = 0;

    while (len < src_size && src[len] && len + 1 < dst_size) len++;
    memcpy( dst, src, len );
    dst[len] = 0;
}

static BOOL pif_read_at( HANDLE file, LONGLONG file_size, DWORD offset, void *buffer, DWORD size )
{
    LARGE_INTEGER pos;
    DWORD read;

    if ((LONGLONG)offset > file_size || size > file_size - offset) return FALSE;
    pos.QuadPart = offset;
    return SetFilePointerEx( file, pos, NULL, FILE_BEGIN ) &&
           ReadFile( file, buffer, size, &read, NULL ) && read == size;
}

BOOL ntvdm_read_pif( const char *path, struct ntvdm_pif *settings )
{
    BYTE header[PIF_HEADER_SIZE], record[PIF_RECORD_SIZE], enhanced[PIF_386_RECORD_SIZE];
    LARGE_INTEGER length;
    HANDLE file;
    DWORD record_offset;
    unsigned int count;
    BOOL found_386 = FALSE, valid = FALSE;

    if (!settings) return FALSE;
    memset( settings, 0, sizeof(*settings) );

    file = CreateFileA( path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL );
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    if (!GetFileSizeEx( file, &length ) ||
        length.QuadPart < PIF_HEADER_SIZE + PIF_RECORD_SIZE ||
        !pif_read_at( file, length.QuadPart, 0, header, sizeof(header) ) ||
        !pif_read_at( file, length.QuadPart, PIF_HEADER_SIZE, record, sizeof(record) ) ||
        memcmp( record, "MICROSOFT PIFEX", 15 ))
        goto done;

    pif_string( settings->program, sizeof(settings->program), header + 0x24, 63 );
    pif_string( settings->title, sizeof(settings->title), header + 0x02, 30 );
    pif_string( settings->directory, sizeof(settings->directory), header + 0x65, 64 );
    pif_string( settings->parameters, sizeof(settings->parameters), header + 0xa5, 64 );
    settings->text_mode = !!(header[0x63] & 0x02);
    settings->close_on_exit = !!(header[0x63] & 0x10);

    /* Each linked record supplies a 16-bit absolute file offset.
     * A bounded traversal also rejects cycles and other corrupt chains. */
    record_offset = PIF_HEADER_SIZE;
    for (count = 0; count < 4096; count++)
    {
        WORD next = pif_word( record + 16 );

        if (next & 0x8000)
        {
            valid = TRUE;
            break;
        }
        if (next < PIF_HEADER_SIZE + PIF_RECORD_SIZE || next == record_offset ||
            !pif_read_at( file, length.QuadPart, next, record, sizeof(record) ))
            break;
        record_offset = next;

        if (!memcmp( record, "WINDOWS 386", 11 ))
        {
            WORD offset = pif_word( record + 18 );
            WORD size = pif_word( record + 20 );

            if (size < sizeof(enhanced) ||
                !pif_read_at( file, length.QuadPart, offset, enhanced, sizeof(enhanced) ))
                break;

            pif_string( settings->parameters, sizeof(settings->parameters), enhanced + 40, 64 );
            settings->text_mode = !!(pif_word( enhanced + 20 ) & 0x0010);
            found_386 = TRUE;
        }
    }

    if (valid)
        TRACE( "PIF %s: program %s, directory %s, 386 extension %u\n",
               debugstr_a(path), debugstr_a(settings->program),
               debugstr_a(settings->directory), found_386 );
done:
    CloseHandle( file );
    if (!valid) SetLastError( ERROR_BAD_FORMAT );
    return valid;
}
