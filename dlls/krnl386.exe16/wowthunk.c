/*
 * Win32 WOW Generic Thunk API
 *
 * Copyright 1999 Ulrich Weigand
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdarg.h>
#include <errno.h>

#include "wine/winbase16.h"
#include "windef.h"
#include "winbase.h"
#include "winerror.h"
#include "wownt32.h"
#include "excpt.h"
#include "winternl.h"
#include "ntgdi.h"
#include "kernel16_private.h"
#include "wine/asm.h"
#include "wine/exception.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(thunk);
WINE_DECLARE_DEBUG_CHANNEL(relay);
WINE_DECLARE_DEBUG_CHANNEL(snoop);

/* symbols exported from relay16.s */
extern DWORD WINAPI wine_call_to_16( FARPROC16 target, DWORD cbArgs, PEXCEPTION_HANDLER handler );
extern void WINAPI wine_call_to_16_regs( I386_CONTEXT *context, DWORD cbArgs, PEXCEPTION_HANDLER handler );
extern void __wine_call_to_16_ret(void);
extern BYTE __wine_call16_start[];
extern BYTE __wine_call16_end[];

static SEGPTR call16_ret_addr;  /* segptr to __wine_call_to_16_ret routine */

#ifdef __i386__
extern const BYTE cbclient_ret[], cbclient_ret_end[];
__ASM_GLOBAL_FUNC( cbclient_ret,
                   "movzwl %sp,%ebx\n\t"
                   "lssl %ss:-16(%ebx),%esp\n\t"
                   "lretl\n\t"
                   ".globl " __ASM_NAME("cbclient_ret_end") "\n"
                   __ASM_NAME("cbclient_ret_end") ":" )

extern const BYTE cbclientex_ret[], cbclientex_ret_end[];
__ASM_GLOBAL_FUNC( cbclientex_ret,
                   "movzwl %bp,%ebx\n\t"
                   "subw %bp,%sp\n\t"
                   "movzwl %sp,%ebp\n\t"
                   "lssl %ss:-12(%ebx),%esp\n\t"
                   "lretl\n\t"
                   ".globl " __ASM_NAME("cbclientex_ret_end") "\n"
                   __ASM_NAME("cbclientex_ret_end") ":" )
#else
/*
 * These are x86 guest return stubs mapped into a selector; they are not
 * native host code.  Encode them as bytes when the host assembler is not x86.
 */
static const BYTE cbclient_ret[] =
{
    0x0f, 0xb7, 0xdc,                   /* movzwl %sp,%ebx */
    0x36, 0x0f, 0xb2, 0x63, 0xf0,       /* lssl %ss:-16(%ebx),%esp */
    0xcb                                /* lretl */
};
#define cbclient_ret_end (cbclient_ret + sizeof(cbclient_ret))

static const BYTE cbclientex_ret[] =
{
    0x0f, 0xb7, 0xdd,                   /* movzwl %bp,%ebx */
    0x66, 0x29, 0xec,                   /* subw %bp,%sp */
    0x0f, 0xb7, 0xec,                   /* movzwl %sp,%ebp */
    0x36, 0x0f, 0xb2, 0x63, 0xf4,       /* lssl %ss:-12(%ebx),%esp */
    0xcb                                /* lretl */
};
#define cbclientex_ret_end (cbclientex_ret + sizeof(cbclientex_ret))
#endif

/***********************************************************************
 *           WOWTHUNK_Init
 */
BOOL WOWTHUNK_Init(void)
{
    /* allocate the code selector for CallTo16 routines */
    WORD codesel = SELECTOR_AllocBlock( __wine_call16_start,
                                        (BYTE *)(&CallTo16_TebSelector + 1) - __wine_call16_start,
                                        code32_segment );

    cbclient_selector = SELECTOR_AllocBlock( cbclient_ret, cbclient_ret_end - cbclient_ret,
                                             code32_segment );
    cbclientex_selector = SELECTOR_AllocBlock( cbclientex_ret, cbclientex_ret_end - cbclientex_ret,
                                               code32_segment );
    if (!codesel || !cbclient_selector || !cbclientex_selector)
        return FALSE;

      /* Patch the return addresses for CallTo16 routines */

    CallTo16_DataSelector = get_ds();
    call16_ret_addr = MAKESEGPTR( codesel, (BYTE *)__wine_call_to_16_ret - __wine_call16_start );

    if (TRACE_ON(relay) || TRACE_ON(snoop)) RELAY16_InitDebugLists();

    return TRUE;
}


/*************************************************************
 *            fix_selector
 *
 * Fix a selector load that caused an exception if it's in the
 * 16-bit relay code.
 */
static BOOL fix_selector( I386_CONTEXT *context )
{
    WORD *stack;
    BYTE *instr = (BYTE *)(UINT_PTR)context->Eip;

    if (instr < __wine_call16_start || instr >= __wine_call16_end) return FALSE;

    /* skip prefixes */
    while (*instr == 0x66 || *instr == 0x67) instr++;

    switch(instr[0])
    {
    case 0x07: /* pop es */
    case 0x1f: /* pop ds */
        break;
    case 0x0f: /* extended instruction */
        switch(instr[1])
        {
        case 0xa1: /* pop fs */
        case 0xa9: /* pop gs */
            break;
        default:
            return FALSE;
        }
        break;
    default:
        return FALSE;
    }
    stack = ldt_get_ptr( context->SegSs, context->Esp );
    TRACE( "fixing up selector %x for pop instruction\n", *stack );
    *stack = 0;
    return TRUE;
}


/*************************************************************
 *            call16_handler
 *
 * Handler for exceptions occurring in 16-bit code.
 */
static DWORD call16_handler( EXCEPTION_RECORD *record, EXCEPTION_REGISTRATION_RECORD *frame,
                             CONTEXT *host_context, EXCEPTION_REGISTRATION_RECORD **pdispatcher )
{
    I386_CONTEXT *context = kernel_get_i386_cpu_context( host_context );

    if (record->ExceptionFlags & (EXCEPTION_UNWINDING | EXCEPTION_EXIT_UNWIND))
    {
        /* unwinding: restore the stack pointer in the TEB, and leave the Win16 mutex */
        STACK32FRAME *frame32 = CONTAINING_RECORD(frame, STACK32FRAME, frame);
        kernel_get_thread_data()->stack = frame32->frame16;
        _LeaveWin16Lock();
    }
    else if (context && (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION ||
                         record->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION))
    {
        if (ldt_is_system(context->SegCs))
        {
            if (fix_selector( context )) return ExceptionContinueExecution;
        }
        else
        {
            SEGPTR gpHandler;
            DWORD ret = __wine_emulate_instruction( record, context );

            if (ret != ExceptionContinueSearch) return ret;

            /* check for Win16 __GP handler */
            if ((gpHandler = HasGPHandler16( MAKESEGPTR( context->SegCs, context->Eip ) )))
            {
                WORD *stack = ldt_get_ptr( context->SegSs, context->Esp );
                *--stack = context->SegCs;
                *--stack = context->Eip;

                if (!ldt_is_32bit(context->SegSs))
                    context->Esp = MAKELONG( LOWORD(context->Esp - 2*sizeof(WORD)),
                                             HIWORD(context->Esp) );
                else
                    context->Esp -= 2*sizeof(WORD);

                context->SegCs = SELECTOROF( gpHandler );
                context->Eip   = OFFSETOF( gpHandler );
                return ExceptionContinueExecution;
            }
        }
    }
    return ExceptionContinueSearch;
}


/*
 *  32-bit WOW routines (in WOW32, but actually forwarded to KERNEL32)
 */

/**********************************************************************
 *           K32WOWGetDescriptor        (KERNEL32.70)
 */
BOOL WINAPI K32WOWGetDescriptor( SEGPTR segptr, LPLDT_ENTRY ldtent )
{
    return GetThreadSelectorEntry( GetCurrentThread(),
                                   segptr >> 16, ldtent );
}

/**********************************************************************
 *           K32WOWGetVDMPointer        (KERNEL32.56)
 */
LPVOID WINAPI K32WOWGetVDMPointer( DWORD vp, DWORD dwBytes, BOOL fProtectedMode )
{
    if (fProtectedMode)
    {
        WORD sel = SELECTOROF( vp );

        /*
         * WOWGetVDMPointer expects a valid LDT selector.  MapSL() assumes the
         * selector is usable and can otherwise turn a stale selector into a
         * bogus linear address.
         *
         * Native retail WOW32 does not use dwBytes for selector-limit checking;
         * that additional check is specific to checked/debug builds.
         */
        if (!ldt_is_valid( sel )) return NULL;
        return MapSL( vp );
    }
    return DOSMEM_MapRealToLinear( vp );
}

/**********************************************************************
 *           K32WOWGetVDMPointerFix     (KERNEL32.68)
 */
LPVOID WINAPI K32WOWGetVDMPointerFix( DWORD vp, DWORD dwBytes, BOOL fProtectedMode )
{
    /*
     * Hmmm. According to the docu, we should call:
     *
     *          GlobalFix16( SELECTOROF(vp) );
     *
     * But this is unnecessary under Wine, as we never move global
     * memory segments in linear memory anyway.
     *
     * (I'm not so sure what we are *supposed* to do if
     *  fProtectedMode is TRUE, anyway ...)
     */

    return K32WOWGetVDMPointer( vp, dwBytes, fProtectedMode );
}

/**********************************************************************
 *           K32WOWGetVDMPointerUnfix   (KERNEL32.69)
 */
VOID WINAPI K32WOWGetVDMPointerUnfix( DWORD vp )
{
    /*
     * See above why we don't call:
     *
     * GlobalUnfix16( SELECTOROF(vp) );
     *
     */
}

/**********************************************************************
 *           K32WOWGlobalAlloc16        (KERNEL32.59)
 */
WORD WINAPI K32WOWGlobalAlloc16( WORD wFlags, DWORD cb )
{
    return (WORD)GlobalAlloc16( wFlags, cb );
}

/**********************************************************************
 *           K32WOWGlobalFree16         (KERNEL32.62)
 */
WORD WINAPI K32WOWGlobalFree16( WORD hMem )
{
    return (WORD)GlobalFree16( (HGLOBAL16)hMem );
}

/**********************************************************************
 *           K32WOWGlobalUnlock16       (KERNEL32.61)
 */
BOOL WINAPI K32WOWGlobalUnlock16( WORD hMem )
{
    return (BOOL)GlobalUnlock16( (HGLOBAL16)hMem );
}

/**********************************************************************
 *           K32WOWGlobalAllocLock16    (KERNEL32.63)
 */
DWORD WINAPI K32WOWGlobalAllocLock16( WORD wFlags, DWORD cb, WORD *phMem )
{
    WORD hMem = K32WOWGlobalAlloc16( wFlags, cb );
    if (phMem) *phMem = hMem;

    return K32WOWGlobalLock16( hMem );
}

/**********************************************************************
 *           K32WOWGlobalLockSize16     (KERNEL32.65)
 */
DWORD WINAPI K32WOWGlobalLockSize16( WORD hMem, PDWORD pcb )
{
    if ( pcb )
        *pcb = GlobalSize16( (HGLOBAL16)hMem );

    return K32WOWGlobalLock16( hMem );
}

/**********************************************************************
 *           K32WOWGlobalUnlockFree16   (KERNEL32.64)
 */
WORD WINAPI K32WOWGlobalUnlockFree16( DWORD vpMem )
{
    if ( !K32WOWGlobalUnlock16( HIWORD(vpMem) ) )
        return FALSE;

    return K32WOWGlobalFree16( HIWORD(vpMem) );
}


/**********************************************************************
 *           K32WOWYield16              (KERNEL32.66)
 */
VOID WINAPI K32WOWYield16( void )
{
    /*
     * This does the right thing for both Win16 and Win32 tasks.
     * More or less, at least :-/
     */
    Yield16();
}

/**********************************************************************
 *           K32WOWDirectedYield16       (KERNEL32.67)
 */
VOID WINAPI K32WOWDirectedYield16( WORD htask16 )
{
    /*
     * Argh.  Our scheduler doesn't like DirectedYield by Win32
     * tasks at all.  So we do hope that this routine is indeed
     * only ever called by Win16 tasks that have thunked up ...
     */
    DirectedYield16( (HTASK16)htask16 );
}

static HANDLE gdi_handle32( WORD handle )
{
    static GDI_SHARED_MEMORY *gdi_shared;

    if (!gdi_shared)
    {
        if (NtCurrentTeb()->GdiBatchCount)
        {
            TEB64 *teb64 = (TEB64 *)(UINT_PTR)NtCurrentTeb()->GdiBatchCount;
            PEB64 *peb64 = (PEB64 *)(UINT_PTR)teb64->Peb;
            gdi_shared = (GDI_SHARED_MEMORY *)(UINT_PTR)peb64->GdiSharedHandleTable;
        }
        else gdi_shared = (GDI_SHARED_MEMORY *)NtCurrentTeb()->Peb->GdiSharedHandleTable;
        if (!gdi_shared) return ULongToHandle( handle );
    }

    return ULongToHandle( (gdi_shared->Handles[handle].Unique << 16) | handle );
}

/***********************************************************************
 *           K32WOWHandle32              (KERNEL32.57)
 */
HANDLE WINAPI K32WOWHandle32( WORD handle, WOW_HANDLE_TYPE type )
{
    switch ( type )
    {
    case WOW_TYPE_HWND:
    case WOW_TYPE_HMENU:
    case WOW_TYPE_HDWP:
    case WOW_TYPE_HDROP:
    case WOW_TYPE_HACCEL:
        return (HANDLE)(ULONG_PTR)handle;

    case WOW_TYPE_HDC:
    case WOW_TYPE_HFONT:
    case WOW_TYPE_HRGN:
    case WOW_TYPE_HBITMAP:
    case WOW_TYPE_HBRUSH:
    case WOW_TYPE_HPALETTE:
    case WOW_TYPE_HPEN:
    case WOW_TYPE_HMETAFILE:
        return gdi_handle32( handle );

    case WOW_TYPE_HTASK:
    {
        TDB *task = GlobalLock16( handle );

        if (!task || !task->teb)
        {
            SetLastError( ERROR_INVALID_HANDLE );
            return NULL;
        }
        return task->teb->ClientId.UniqueThread;
    }

    case WOW_TYPE_FULLHWND:
        FIXME( "conversion of full window handles not supported yet\n" );
        return (HANDLE)(ULONG_PTR)handle;

    default:
        ERR( "handle 0x%04x of unknown type %d\n", handle, type );
        return (HANDLE)(ULONG_PTR)handle;
    }
}

/***********************************************************************
 *           K32WOWHandle16              (KERNEL32.58)
 */
WORD WINAPI K32WOWHandle16( HANDLE handle, WOW_HANDLE_TYPE type )
{
    switch ( type )
    {
    case WOW_TYPE_HWND:
    case WOW_TYPE_HMENU:
    case WOW_TYPE_HDWP:
    case WOW_TYPE_HDROP:
    case WOW_TYPE_HDC:
    case WOW_TYPE_HFONT:
    case WOW_TYPE_HRGN:
    case WOW_TYPE_HBITMAP:
    case WOW_TYPE_HBRUSH:
    case WOW_TYPE_HPALETTE:
    case WOW_TYPE_HPEN:
    case WOW_TYPE_HACCEL:
    case WOW_TYPE_FULLHWND:
    	if ( HIWORD(handle ) )
        	ERR( "handle %p of type %d has non-zero HIWORD\n", handle, type );
        return LOWORD(handle);

    case WOW_TYPE_HMETAFILE:
        FIXME( "conversion of metafile handles not supported yet\n" );
        return LOWORD(handle);

    case WOW_TYPE_HTASK:
        return TASK_GetTaskFromThread( (DWORD)handle );

    default:
        ERR( "handle %p of unknown type %d\n", handle, type );
        return LOWORD(handle);
    }
}

/**********************************************************************
 *           K32WOWCallback16Ex         (KERNEL32.55)
 */
BOOL WINAPI K32WOWCallback16Ex( DWORD vpfn16, DWORD dwFlags,
                                DWORD cbArgs, LPVOID pArgs, LPDWORD pdwRetCode )
{
    if (cbArgs > WCB16_MAX_CBARGS)
    {
        SetLastError( ERROR_INVALID_PARAMETER );
        return FALSE;
    }

    /*
     * Arguments must be prepared in the correct order by the caller
     * (both for PASCAL and CDECL calling convention), so we simply
     * copy them to the 16-bit stack ...
     */
    char *stack = (char *)CURRENT_STACK16 - cbArgs;

    memcpy( stack, pArgs, cbArgs );

    if (dwFlags & WCB16_REGS)
    {
        I386_CONTEXT *context = (I386_CONTEXT *)pdwRetCode;

        if (TRACE_ON(relay))
        {
            DWORD count = cbArgs / sizeof(WORD);
            WORD * wstack = (WORD *)stack;

            TRACE_(relay)( "\1CallTo16(func=%04lx:%04x", context->SegCs, LOWORD(context->Eip) );
            while (count) TRACE_(relay)( ",%04x", wstack[--count] );
            TRACE_(relay)( ") ss:sp=%04x:%04x ax=%04x bx=%04x cx=%04x dx=%04x si=%04x di=%04x bp=%04x ds=%04x es=%04x\n",
                           CURRENT_SS, CURRENT_SP,
                           (WORD)context->Eax, (WORD)context->Ebx, (WORD)context->Ecx,
                           (WORD)context->Edx, (WORD)context->Esi, (WORD)context->Edi,
                           (WORD)context->Ebp, (WORD)context->SegDs, (WORD)context->SegEs );
            SYSLEVEL_CheckNotLevel( 2 );
        }

        /* push return address */
        stack -= sizeof(SEGPTR);
        *((SEGPTR *)stack) = call16_ret_addr;
        cbArgs += sizeof(SEGPTR);

        if (!(dwFlags & WCB16_INTERRUPT)) _EnterWin16Lock();
        wine_call_to_16_regs( context, cbArgs, call16_handler );
        if (!(dwFlags & WCB16_INTERRUPT)) _LeaveWin16Lock();

        if (TRACE_ON(relay))
        {
            TRACE_(relay)( "\1RetFrom16() ss:sp=%04x:%04x ax=%04x bx=%04x cx=%04x dx=%04x bp=%04x sp=%04x\n",
                           CURRENT_SS, CURRENT_SP,
                           (WORD)context->Eax, (WORD)context->Ebx, (WORD)context->Ecx,
                           (WORD)context->Edx, (WORD)context->Ebp, (WORD)context->Esp );
            SYSLEVEL_CheckNotLevel( 2 );
        }
    }
    else
    {
        DWORD ret;

        if (TRACE_ON(relay))
        {
            DWORD count = cbArgs / sizeof(WORD);
            WORD * wstack = (WORD *)stack;

            TRACE_(relay)( "\1CallTo16(func=%04x:%04x,ds=%04x",
                           HIWORD(vpfn16), LOWORD(vpfn16), CURRENT_SS );
            while (count) TRACE_(relay)( ",%04x", wstack[--count] );
            TRACE_(relay)( ") ss:sp=%04x:%04x\n", CURRENT_SS, CURRENT_SP );
            SYSLEVEL_CheckNotLevel( 2 );
        }

        /* push return address */
        stack -= sizeof(SEGPTR);
        *((SEGPTR *)stack) = call16_ret_addr;
        cbArgs += sizeof(SEGPTR);

        /*
         * Actually, we should take care whether the called routine cleans up
         * its stack or not.  Fortunately, our wine_call_to_16 core doesn't rely on
         * the callee to do so; after the routine has returned, the 16-bit
         * stack pointer is always reset to the position it had before.
         */
        if (!(dwFlags & WCB16_INTERRUPT)) _EnterWin16Lock();
        ret = wine_call_to_16( (FARPROC16)vpfn16, cbArgs, call16_handler );
        if (pdwRetCode) *pdwRetCode = ret;
        if (!(dwFlags & WCB16_INTERRUPT)) _LeaveWin16Lock();

        if (TRACE_ON(relay))
        {
            TRACE_(relay)( "\1RetFrom16() ss:sp=%04x:%04x retval=%08lx\n", CURRENT_SS, CURRENT_SP, ret );
            SYSLEVEL_CheckNotLevel( 2 );
        }
    }

    return TRUE;  /* success */
}

/**********************************************************************
 *           K32WOWCallback16            (KERNEL32.54)
 */
DWORD WINAPI K32WOWCallback16( DWORD vpfn16, DWORD dwParam )
{
    DWORD ret;

    if ( !K32WOWCallback16Ex( vpfn16, WCB16_PASCAL,
                           sizeof(DWORD), &dwParam, &ret ) )
        ret = 0L;

    return ret;
}


/**********************************************************************
 *           WOWMsgBox16  (NT KERNEL.263)
 *
 * OpenNT exposes:
 *   void FAR PASCAL WowMsgBox(LPSTR msg, LPSTR title, DWORD style)
 * and immediately thunks the work to WOW32.  Validate/map the Win16 strings
 * here; WOW32 copies them before returning so no guest pointer escapes.
 */
void WINAPI WOWMsgBox16( SEGPTR msg_ptr, SEGPTR title_ptr, DWORD style )
{
    typedef void (__cdecl *wow_msgbox_proc)(const char *, const char *, DWORD);
    static wow_msgbox_proc msgbox;
    const char *msg = NULL, *title = NULL;
    HMODULE module;

    if (msg_ptr)
    {
        if (IsBadStringPtr16( msg_ptr, 0xffff )) return;
        msg = K32WOWGetVDMPointer( msg_ptr, 1, TRUE );
        if (!msg) return;
    }
    if (title_ptr)
    {
        if (IsBadStringPtr16( title_ptr, 0xffff )) return;
        title = K32WOWGetVDMPointer( title_ptr, 1, TRUE );
        if (!title) return;
    }

    if (!msgbox)
    {
        module = GetModuleHandleA( "wow32.dll" );
        if (module)
            msgbox = (wow_msgbox_proc)GetProcAddress( module, "__wine_WOWMsgBox" );
    }

    if (!msgbox)
    {
        WARN( "WOW32 message-box bridge is unavailable\n" );
        return;
    }

    msgbox( msg, title, style );
}


/**********************************************************************
 *           WOWShouldWeSayWin9516  (NT KERNEL.215)
 *
 * NT reuses ordinal 215, which is Local32ValidHandle on Win95. OpenNT shows
 * this as a WOW32 compatibility-policy thunk taking (filename, caller DS).
 * Keep only the 16-bit pointer conversion here; host compatibility policy is
 * owned by WOW32.
 */
WORD WINAPI WOWShouldWeSayWin9516( SEGPTR filename_ptr, WORD caller_ds )
{
    typedef DWORD (__cdecl *wow_should_say_win95_proc)(const char *, DWORD);
    static wow_should_say_win95_proc should_say_win95;
    const char *filename = NULL;
    HMODULE module;

    if (filename_ptr)
    {
        filename = K32WOWGetVDMPointer( filename_ptr, 1, TRUE );
        if (!filename) return 0;
    }

    if (!should_say_win95)
    {
        module = GetModuleHandleA( "wow32.dll" );
        if (module)
            should_say_win95 = (wow_should_say_win95_proc)GetProcAddress(
                module, "__wine_WOWShouldWeSayWin95" );
    }

    if (!should_say_win95) return 0;
    return LOWORD( should_say_win95( filename, caller_ds ) );
}


/**********************************************************************
 *           WOWRegisterShellWindowHandle16  (NT KERNEL.503)
 *
 * OpenNT declares this as:
 *   WOWRegisterShellWindowHandle(HWND, LPVOID, HWND)
 * and routes it through WOW32. The middle command-show pointer was already
 * unused by native WOW32; preserve it in the ABI without dereferencing it.
 */
BOOL16 WINAPI WOWRegisterShellWindowHandle16( WORD hwnd_shell, SEGPTR cmd_show, WORD hwnd_fax )
{
    typedef BOOL (__cdecl *wow_register_shell_proc)(HWND, HWND, DWORD);
    static wow_register_shell_proc register_shell;
    HMODULE module;

    (void)cmd_show;

    if (!register_shell)
    {
        module = GetModuleHandleA( "wow32.dll" );
        if (module)
            register_shell = (wow_register_shell_proc)GetProcAddress(
                module, "__wine_WOWRegisterShellWindow" );
    }

    if (!register_shell)
    {
        WARN( "WOW32 shell registration bridge is unavailable\n" );
        return FALSE;
    }

    return register_shell( (HWND)K32WOWHandle32( hwnd_shell, WOW_TYPE_HWND ),
                           (HWND)K32WOWHandle32( hwnd_fax, WOW_TYPE_HWND ),
                           GetCurrentTask() );
}

/**********************************************************************
 *           WOWQueryPerformanceCounter16  (NT KERNEL.505)
 *
 * OpenNT's WOW thunk frame stores the second Pascal argument first:
 * frequency, then counter. The callable ABI is therefore
 * (counter, frequency), matching the source-level argument order here.
 */
BOOL16 WINAPI WOWQueryPerformanceCounter16( SEGPTR counter_ptr, SEGPTR frequency_ptr )
{
    typedef BOOL (__cdecl *wow_query_counter_proc)(LARGE_INTEGER *, LARGE_INTEGER *);
    static wow_query_counter_proc query_counter;
    LARGE_INTEGER *counter = NULL, *frequency = NULL;
    HMODULE module;

    if (counter_ptr)
    {
        counter = K32WOWGetVDMPointer( counter_ptr, sizeof(*counter), TRUE );
        if (!counter) return FALSE;
    }
    if (frequency_ptr)
    {
        frequency = K32WOWGetVDMPointer( frequency_ptr, sizeof(*frequency), TRUE );
        if (!frequency) return FALSE;
    }

    if (!query_counter)
    {
        module = GetModuleHandleA( "wow32.dll" );
        if (module)
            query_counter = (wow_query_counter_proc)GetProcAddress(
                module, "__wine_WOWQueryPerformanceCounter" );
    }

    if (!query_counter)
    {
        WARN( "WOW32 performance-counter bridge is unavailable\n" );
        return FALSE;
    }

    return query_counter( counter, frequency );
}


/**********************************************************************
 *           WOWKillRemoteTask16       (KERNEL.511)
 *
 * Register/poll the WOWDEB communication block with WOW32.  Native NT
 * never returns from the initial call; Water keeps WOWDEB as a cooperative
 * Win16 task until the cross-process VDMDBG remote-thread path is complete.
 */
BOOL16 WINAPI WOWKillRemoteTask16( SEGPTR block )
{
    typedef BOOL (__cdecl *wowdebug_poll_proc)(DWORD);
    static wowdebug_poll_proc poll;
    HMODULE module;

    if (block && !ldt_is_valid( SELECTOROF(block) ))
    {
        WARN( "invalid WOWDEB communication block %08lx\n", block );
        return FALSE;
    }

    if (!poll)
    {
        /*
         * NTVDM owns WOW32 lifetime.  Do not load it as a side effect of a
         * KERNEL debug export; the host must have initialized WOW32 already.
         */
        module = GetModuleHandleA( "wow32.dll" );
        if (module) poll = (wowdebug_poll_proc)GetProcAddress( module, "__wine_WOWDebugPoll16" );
    }

    if (!poll)
    {
        WARN( "WOW32 debugging bridge is unavailable\n" );
        return FALSE;
    }

    return poll( block );
}


/**********************************************************************
 *           WOWQueryDebug16           (KERNEL.512)
 *
 * Bit 0 is the NT WOW DebugWOW flag: a 32-bit debugger is attached.
 */
WORD WINAPI WOWQueryDebug16( void )
{
    typedef DWORD (__cdecl *wowdebug_query_proc)(void);
    static wowdebug_query_proc query;
    HMODULE module;

    if (!query)
    {
        module = GetModuleHandleA( "wow32.dll" );
        if (module)
            query = (wowdebug_query_proc)GetProcAddress( module, "__wine_WOWQueryDebug16" );
    }

    if (!query)
    {
        WARN( "WOW32 debugging state bridge is unavailable\n" );
        return 0;
    }

    return LOWORD( query() );
}
