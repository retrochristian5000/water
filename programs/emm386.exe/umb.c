/*
 * EMM386 upper-memory provider compatibility.
 *
 * Microsoft EMM386 uses XMS memory to provide EMS and/or UMBs.  When UMBs
 * are enabled EMM386 answers the XMS UMB allocation/deallocation services;
 * HIMEM.SYS continues to own the rest of XMS.
 *
 * This file currently implements only the UMB-provider portion recovered
 * from KRNL386. EMS INT 67h, the EMS page frame and VCPI remain separate
 * future EMM386 work.
 */

#include "windef.h"
#include "winbase.h"
#include "wine/debug.h"
#include "dosexe.h"

#include "emm386.h"

WINE_DEFAULT_DEBUG_CHANNEL(emm386);

#define EMM386_MAX_UMB_HANDLES 128

#define XMS_ERR_UMB_SMALLER    0xb0
#define XMS_ERR_UMB_NONE       0xb1
#define XMS_ERR_UMB_INVALID    0xb2

static WORD emm386_umb_segments[EMM386_MAX_UMB_HANDLES];

extern LPVOID DOSMEM_AllocBlockHigh(UINT size, UINT16 *pseg, BYTE strategy);
extern BOOL DOSMEM_FreeBlock(void *ptr);
extern UINT DOSMEM_AvailableHigh(void);
extern LPVOID DOSMEM_MapDosToLinear(UINT ptr);

static void emm386_xms_success(I386_CONTEXT *context)
{
    SET_AX( context, 1 );
    SET_BL( context, 0 );
}

static void emm386_xms_failure(I386_CONTEXT *context, BYTE error)
{
    SET_AX( context, 0 );
    SET_BL( context, error );
}

static BOOL emm386_track_umb(WORD segment)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(emm386_umb_segments); i++)
    {
        if (!emm386_umb_segments[i])
        {
            emm386_umb_segments[i] = segment;
            return TRUE;
        }
    }
    return FALSE;
}

static BOOL emm386_owns_umb(WORD segment, unsigned int *index)
{
    unsigned int i;

    for (i = 0; i < ARRAY_SIZE(emm386_umb_segments); i++)
    {
        if (emm386_umb_segments[i] == segment)
        {
            if (index) *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

void EMM386_XMSRequestUMB(I386_CONTEXT *context)
{
    UINT paragraphs = DX_reg(context);
    WORD segment = 0;

    if (DOSMEM_AllocBlockHigh( paragraphs << 4, &segment, 0 ))
    {
        if (!emm386_track_umb( segment ))
        {
            DOSMEM_FreeBlock( DOSMEM_MapDosToLinear( (UINT)segment << 4 ) );
            SET_AX( context, 0 );
            SET_DX( context, DOSMEM_AvailableHigh() >> 4 );
            SET_BL( context, XMS_ERR_UMB_NONE );
            return;
        }

        SET_AX( context, 1 );
        SET_BX( context, segment );
        SET_DX( context, paragraphs );
        TRACE( "allocated XMS UMB %04x:%04x paragraphs\n", segment, paragraphs );
        return;
    }

    {
        UINT largest = DOSMEM_AvailableHigh() >> 4;

        SET_AX( context, 0 );
        SET_DX( context, largest );
        SET_BL( context, largest ? XMS_ERR_UMB_SMALLER : XMS_ERR_UMB_NONE );
    }
}

void EMM386_XMSReleaseUMB(I386_CONTEXT *context)
{
    WORD segment = DX_reg(context);
    unsigned int index;

    if (!emm386_owns_umb( segment, &index ) ||
        !DOSMEM_FreeBlock( DOSMEM_MapDosToLinear( (UINT)segment << 4 ) ))
    {
        emm386_xms_failure( context, XMS_ERR_UMB_INVALID );
        return;
    }

    emm386_umb_segments[index] = 0;
    emm386_xms_success( context );
    TRACE( "released XMS UMB %04x\n", segment );
}
