/*
 * Windows NT Win16 debugging helper
 *
 * This follows the WOWDEB communication-block protocol used by NTVDM and
 * VDMDBG.  Water keeps the helper resident while the remote transport is
 * being completed, rather than terminating its Win16 task as native NT does.
 */

#include <stdarg.h>
#include <string.h>

#include "windef.h"
#include "winbase.h"
#include "wownt32.h"
#include "wine/winbase16.h"
#include "wine/wowdeb.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wowdeb);

extern BOOL16 WINAPI WOWKillRemoteTask16( SEGPTR block );

static BOOL get_remote_names( BYTE *block, WOWDEB_COM_HEADER *header,
                              char **module_name, char **entry_name )
{
    BYTE *end = block + WOWDEB_COMM_BLOCK_SIZE;
    char *module, *entry;
    void *nul;

    if (header->wArgsPassed > header->wArgsSize) return FALSE;
    if (header->wArgsSize > WOWDEB_COMM_BLOCK_SIZE - sizeof(*header)) return FALSE;

    module = (char *)block + sizeof(*header) + header->wArgsSize;
    if ((BYTE *)module >= end) return FALSE;

    nul = memchr( module, 0, end - (BYTE *)module );
    if (!nul) return FALSE;

    entry = (char *)nul + 1;
    if ((BYTE *)entry >= end) return FALSE;
    if (!memchr( entry, 0, end - (BYTE *)entry )) return FALSE;

    *module_name = module;
    *entry_name = entry;
    return TRUE;
}

/**************************************************************************
 *           WOWDEB entry point
 */
WORD WINAPI WinMain16( HINSTANCE16 inst, HINSTANCE16 prev, LPSTR cmdline, WORD show )
{
    HGLOBAL16 block_handle;
    HINSTANCE16 module;
    WOWDEB_COM_HEADER *header;
    BYTE *block;
    SEGPTR block16;
    FARPROC16 proc;
    char *module_name, *entry_name;
    DWORD ret = WOWDEB_DEAD_VALUE;

    if (prev) return FALSE;

    if (!(block_handle = GlobalAlloc16( GMEM_FIXED | GMEM_ZEROINIT, WOWDEB_COMM_BLOCK_SIZE )))
    {
        ERR( "failed to allocate WOWDEB communication block\n" );
        return FALSE;
    }

    if (!(block = GlobalLock16( block_handle )))
    {
        ERR( "failed to lock WOWDEB communication block\n" );
        GlobalFree16( block_handle );
        return FALSE;
    }

    block16 = MAKESEGPTR( GlobalHandleToSel16( block_handle ), 0 );

    /* Native WOWDEB preloads TOOLHELP because remote calls target it. */
    LoadLibrary16( "TOOLHELP.DLL" );

    header = (WOWDEB_COM_HEADER *)block;
    header->dwBlockAddress = block16;
    header->dwReturnValue = WOWDEB_DEAD_VALUE;
    header->wArgsPassed = 0;
    header->wArgsSize = 0;
    header->wBlockLength = WOWDEB_COMM_BLOCK_SIZE;
    header->wSuccess = FALSE;

    TRACE( "registered communication block %08lx (%p)\n", block16, block );

    for (;;)
    {
        /*
         * KERNEL.511 registers/polls the remote helper endpoint.  Native NT
         * destroys this task and later re-enters it through W32RemoteThread.
         * Water keeps it cooperative and resident until the VDMDBG transport
         * can drive the same protocol across processes.
         */
        if (!WOWKillRemoteTask16( block16 ))
        {
            /*
             * Native NT removes WOWDEB from the ordinary task count when it
             * hands the helper context to WOW32.  Water keeps the helper live,
             * so explicitly leave once it is the final Win16 task.
             */
            if (GetNumTasks16() <= 1) break;
            WOWYield16();
            continue;
        }

        header->wSuccess = FALSE;
        header->dwReturnValue = 0;

        if (!get_remote_names( block, header, &module_name, &entry_name ))
        {
            WARN( "invalid WOWDEB communication block\n" );
            continue;
        }

        if ((module = LoadLibrary16( module_name )) < 32)
        {
            WARN( "failed to load remote module %s\n", debugstr_a(module_name) );
            continue;
        }

        if (!(proc = GetProcAddress16( module, entry_name )))
        {
            WARN( "failed to resolve %s!%s\n", debugstr_a(module_name), debugstr_a(entry_name) );
            continue;
        }

        if (!WOWCallback16Ex( (DWORD)proc, WCB16_PASCAL, header->wArgsPassed,
                              block + sizeof(*header), &ret ))
        {
            WARN( "remote Win16 call %s!%s failed\n",
                  debugstr_a(module_name), debugstr_a(entry_name) );
            continue;
        }

        header->dwReturnValue = ret;
        header->wSuccess = TRUE;
    }

    /* Clear WOW32's process-local registration before releasing the block. */
    WOWKillRemoteTask16( 0 );
    GlobalFree16( block_handle );
    return 0;
}
