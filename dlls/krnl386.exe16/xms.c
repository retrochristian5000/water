/*
 * XMS 2.0 compatibility for Win16 DOS services
 *
 * Microsoft HIMEM.SYS exposes this interface through INT 2fh/AX=4300h
 * and AX=4310h.  Water does not load CONFIG.SYS device drivers, so the
 * surviving Win16 DOS layer provides the XMS control entry directly.
 */

#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wine/winbase16.h"
#include "wine/debug.h"
#include "dosexe.h"

WINE_DEFAULT_DEBUG_CHANNEL(xms);

#define XMS_VERSION              0x0200
#define XMS_MAX_HANDLES          128
#define XMS_DOSMEM_LIMIT         0x00110000u

#define XMS_ERR_NOT_IMPLEMENTED  0x80
#define XMS_ERR_A20              0x82
#define XMS_ERR_HMA_NOEXIST      0x90
#define XMS_ERR_HMA_IN_USE       0x91
#define XMS_ERR_HMA_NOT_USED     0x93
#define XMS_ERR_A20_STILL_ON     0x94
#define XMS_ERR_OUT_OF_MEMORY    0xa0
#define XMS_ERR_OUT_OF_HANDLES   0xa1
#define XMS_ERR_INVALID_HANDLE   0xa2
#define XMS_ERR_SRC_HANDLE       0xa3
#define XMS_ERR_SRC_OFFSET       0xa4
#define XMS_ERR_DST_HANDLE       0xa5
#define XMS_ERR_DST_OFFSET       0xa6
#define XMS_ERR_LENGTH           0xa7
#define XMS_ERR_NOT_LOCKED       0xaa
#define XMS_ERR_LOCKED           0xab
#define XMS_ERR_LOCK_OVERFLOW    0xac
#define XMS_ERR_LOCK_FAILED      0xad
#define XMS_ERR_UMB_SMALLER      0xb0
#define XMS_ERR_UMB_NONE         0xb1
#define XMS_ERR_UMB_INVALID      0xb2

struct xms_block
{
    HGLOBAL16 global;
    DWORD size;
    BYTE locks;
};

#pragma pack(push,1)
struct xms_move
{
    DWORD length;
    WORD source_handle;
    DWORD source_offset;
    WORD dest_handle;
    DWORD dest_offset;
};
#pragma pack(pop)

static struct xms_block xms_blocks[XMS_MAX_HANDLES];
static WORD xms_umb_segments[XMS_MAX_HANDLES];
static BOOL xms_hma_in_use;
static BOOL xms_global_a20;
static BYTE xms_local_a20;

static void xms_success(I386_CONTEXT *context)
{
    SET_AX( context, 1 );
    SET_BL( context, 0 );
}

static void xms_failure(I386_CONTEXT *context, BYTE error)
{
    SET_AX( context, 0 );
    SET_BL( context, error );
}

static struct xms_block *xms_get_block(WORD handle)
{
    if (!handle || handle > XMS_MAX_HANDLES) return NULL;
    if (!xms_blocks[handle - 1].global) return NULL;
    return &xms_blocks[handle - 1];
}

static WORD xms_free_handles(void)
{
    WORD count = 0;
    unsigned int i;

    for (i = 0; i < XMS_MAX_HANDLES; i++)
        if (!xms_blocks[i].global) count++;
    return count;
}

static WORD xms_find_free_handle(void)
{
    unsigned int i;

    for (i = 0; i < XMS_MAX_HANDLES; i++)
        if (!xms_blocks[i].global) return i + 1;
    return 0;
}

static BOOL xms_track_umb(WORD segment)
{
    unsigned int i;

    for (i = 0; i < XMS_MAX_HANDLES; i++)
    {
        if (!xms_umb_segments[i])
        {
            xms_umb_segments[i] = segment;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL xms_owns_umb(WORD segment, unsigned int *index)
{
    unsigned int i;

    for (i = 0; i < XMS_MAX_HANDLES; i++)
    {
        if (xms_umb_segments[i] == segment)
        {
            if (index) *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

static DWORD xms_available_kb(void)
{
    MEMORYSTATUS status;
    DWORD available;

    memset( &status, 0, sizeof(status) );
    status.dwLength = sizeof(status);
    GlobalMemoryStatus( &status );

    available = status.dwAvailVirtual >> 10;
    if (available > 0xffff) available = 0xffff;
    return available;
}

static BOOL xms_move_pointer(WORD handle, DWORD offset, DWORD length,
                             BYTE handle_error, BYTE offset_error,
                             BYTE **ptr, HGLOBAL16 *locked, BYTE *error)
{
    struct xms_block *block;
    DWORD linear;

    *ptr = NULL;
    *locked = 0;

    if (!handle)
    {
        linear = ((DWORD)HIWORD(offset) << 4) + LOWORD(offset);
        if (linear >= XMS_DOSMEM_LIMIT || length > XMS_DOSMEM_LIMIT - linear)
        {
            *error = offset_error;
            return FALSE;
        }
        *ptr = DOSMEM_MapDosToLinear( linear );
        return TRUE;
    }

    if (!(block = xms_get_block( handle )))
    {
        *error = handle_error;
        return FALSE;
    }
    if (offset > block->size || length > block->size - offset)
    {
        *error = offset_error;
        return FALSE;
    }

    if (!(*ptr = GlobalLock16( block->global )))
    {
        *error = handle_error;
        return FALSE;
    }
    *ptr += offset;
    *locked = block->global;
    return TRUE;
}

void DOSVM_XMSHandler(I386_CONTEXT *context)
{
    TRACE("function %02x\n", AH_reg(context));

    switch (AH_reg(context))
    {
    case 0x00:  /* Get XMS version number */
        SET_AX( context, XMS_VERSION );
        SET_BX( context, 0 );
        SET_DX( context, 1 );  /* Water maps the 64K HMA. */
        break;

    case 0x01:  /* Request HMA */
        if (xms_hma_in_use)
            xms_failure( context, XMS_ERR_HMA_IN_USE );
        else
        {
            xms_hma_in_use = TRUE;
            xms_success( context );
        }
        break;

    case 0x02:  /* Release HMA */
        if (!xms_hma_in_use)
            xms_failure( context, XMS_ERR_HMA_NOT_USED );
        else
        {
            xms_hma_in_use = FALSE;
            xms_success( context );
        }
        break;

    case 0x03:  /* Global Enable A20 */
        xms_global_a20 = TRUE;
        xms_success( context );
        break;

    case 0x04:  /* Global Disable A20 */
        xms_global_a20 = FALSE;
        if (xms_local_a20)
            xms_failure( context, XMS_ERR_A20_STILL_ON );
        else
            xms_success( context );
        break;

    case 0x05:  /* Local Enable A20 */
        if (xms_local_a20 == 0xff)
            xms_failure( context, XMS_ERR_A20 );
        else
        {
            xms_local_a20++;
            xms_success( context );
        }
        break;

    case 0x06:  /* Local Disable A20 */
        if (!xms_local_a20)
            xms_failure( context, XMS_ERR_A20_STILL_ON );
        else
        {
            xms_local_a20--;
            if (xms_global_a20 || xms_local_a20)
                xms_failure( context, XMS_ERR_A20_STILL_ON );
            else
                xms_success( context );
        }
        break;

    case 0x07:  /* Query A20 */
        SET_AX( context, (xms_global_a20 || xms_local_a20) ? 1 : 0 );
        SET_BL( context, 0 );
        break;

    case 0x08:  /* Query Free Extended Memory */
    {
        DWORD available = xms_available_kb();

        SET_AX( context, available );
        SET_DX( context, available );
        SET_BL( context, available ? 0 : XMS_ERR_OUT_OF_MEMORY );
        break;
    }

    case 0x09:  /* Allocate Extended Memory Block */
    {
        WORD handle = xms_find_free_handle();
        DWORD size = (DWORD)DX_reg(context) << 10;
        HGLOBAL16 global;

        if (!handle)
        {
            SET_DX( context, 0 );
            xms_failure( context, XMS_ERR_OUT_OF_HANDLES );
            break;
        }

        global = GlobalAlloc16( GMEM_MOVEABLE, size ? size : 1 );
        if (!global)
        {
            SET_DX( context, 0 );
            xms_failure( context, XMS_ERR_OUT_OF_MEMORY );
            break;
        }

        xms_blocks[handle - 1].global = global;
        xms_blocks[handle - 1].size = size;
        xms_blocks[handle - 1].locks = 0;
        SET_DX( context, handle );
        xms_success( context );
        break;
    }

    case 0x0a:  /* Free Extended Memory Block */
    {
        struct xms_block *block = xms_get_block( DX_reg(context) );

        if (!block)
        {
            xms_failure( context, XMS_ERR_INVALID_HANDLE );
            break;
        }
        if (block->locks)
        {
            xms_failure( context, XMS_ERR_LOCKED );
            break;
        }
        if (GlobalFree16( block->global ))
        {
            xms_failure( context, XMS_ERR_INVALID_HANDLE );
            break;
        }
        memset( block, 0, sizeof(*block) );
        xms_success( context );
        break;
    }

    case 0x0b:  /* Move Extended Memory Block */
    {
        struct xms_move *move = ldt_get_ptr( context->SegDs, SI_reg(context) );
        BYTE *source, *dest, error;
        HGLOBAL16 source_lock, dest_lock;

        if (!move || (move->length & 1))
        {
            xms_failure( context, XMS_ERR_LENGTH );
            break;
        }

        if (!xms_move_pointer( move->source_handle, move->source_offset, move->length,
                               XMS_ERR_SRC_HANDLE, XMS_ERR_SRC_OFFSET,
                               &source, &source_lock, &error ))
        {
            xms_failure( context, error );
            break;
        }

        if (!xms_move_pointer( move->dest_handle, move->dest_offset, move->length,
                               XMS_ERR_DST_HANDLE, XMS_ERR_DST_OFFSET,
                               &dest, &dest_lock, &error ))
        {
            if (source_lock) GlobalUnlock16( source_lock );
            xms_failure( context, error );
            break;
        }

        memmove( dest, source, move->length );
        if (dest_lock) GlobalUnlock16( dest_lock );
        if (source_lock) GlobalUnlock16( source_lock );
        xms_success( context );
        break;
    }

    case 0x0c:  /* Lock Extended Memory Block */
    {
        struct xms_block *block = xms_get_block( DX_reg(context) );
        LPVOID ptr;
        ULONG_PTR linear;

        if (!block)
        {
            xms_failure( context, XMS_ERR_INVALID_HANDLE );
            break;
        }
        if (block->locks == 0xff)
        {
            xms_failure( context, XMS_ERR_LOCK_OVERFLOW );
            break;
        }
        if (!(ptr = GlobalLock16( block->global )))
        {
            xms_failure( context, XMS_ERR_LOCK_FAILED );
            break;
        }

        linear = (ULONG_PTR)ptr;
        if (linear > 0xffffffffu)
        {
            GlobalUnlock16( block->global );
            xms_failure( context, XMS_ERR_LOCK_FAILED );
            break;
        }

        block->locks++;
        SET_AX( context, 1 );
        SET_BX( context, LOWORD((DWORD)linear) );
        SET_DX( context, HIWORD((DWORD)linear) );
        break;
    }

    case 0x0d:  /* Unlock Extended Memory Block */
    {
        struct xms_block *block = xms_get_block( DX_reg(context) );

        if (!block)
            xms_failure( context, XMS_ERR_INVALID_HANDLE );
        else if (!block->locks)
            xms_failure( context, XMS_ERR_NOT_LOCKED );
        else
        {
            GlobalUnlock16( block->global );
            block->locks--;
            xms_success( context );
        }
        break;
    }

    case 0x0e:  /* Get EMB Handle Information */
    {
        struct xms_block *block = xms_get_block( DX_reg(context) );

        if (!block)
        {
            xms_failure( context, XMS_ERR_INVALID_HANDLE );
            break;
        }

        SET_AX( context, 1 );
        SET_BH( context, block->locks );
        SET_BL( context, xms_free_handles() );
        SET_DX( context, block->size >> 10 );
        break;
    }

    case 0x0f:  /* Reallocate Extended Memory Block */
    {
        struct xms_block *block = xms_get_block( DX_reg(context) );
        DWORD size = (DWORD)BX_reg(context) << 10;
        HGLOBAL16 global;

        if (!block)
        {
            xms_failure( context, XMS_ERR_INVALID_HANDLE );
            break;
        }
        if (block->locks)
        {
            xms_failure( context, XMS_ERR_LOCKED );
            break;
        }

        global = GlobalReAlloc16( block->global, size ? size : 1, GMEM_MOVEABLE );
        if (!global)
        {
            xms_failure( context, XMS_ERR_OUT_OF_MEMORY );
            break;
        }

        block->global = global;
        block->size = size;
        xms_success( context );
        break;
    }

    case 0x10:  /* Request Upper Memory Block */
    {
        UINT paragraphs = DX_reg(context);
        WORD segment = 0;

        if (DOSMEM_AllocBlockHigh( paragraphs << 4, &segment, 0 ))
        {
            if (!xms_track_umb( segment ))
            {
                DOSMEM_FreeBlock( DOSMEM_MapDosToLinear( (UINT)segment << 4 ) );
                SET_AX( context, 0 );
                SET_DX( context, DOSMEM_AvailableHigh() >> 4 );
                SET_BL( context, XMS_ERR_UMB_NONE );
                break;
            }

            SET_AX( context, 1 );
            SET_BX( context, segment );
            SET_DX( context, paragraphs );
        }
        else
        {
            UINT largest = DOSMEM_AvailableHigh() >> 4;

            SET_AX( context, 0 );
            SET_DX( context, largest );
            SET_BL( context, largest ? XMS_ERR_UMB_SMALLER : XMS_ERR_UMB_NONE );
        }
        break;
    }

    case 0x11:  /* Release Upper Memory Block */
    {
        WORD segment = DX_reg(context);
        unsigned int index;

        if (!xms_owns_umb( segment, &index ) ||
            !DOSMEM_FreeBlock( DOSMEM_MapDosToLinear( (UINT)segment << 4 ) ))
            xms_failure( context, XMS_ERR_UMB_INVALID );
        else
        {
            xms_umb_segments[index] = 0;
            xms_success( context );
        }
        break;
    }

    default:
        WARN("XMS function %02x not implemented\n", AH_reg(context));
        xms_failure( context, XMS_ERR_NOT_IMPLEMENTED );
        break;
    }
}
