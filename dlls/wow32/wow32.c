/*
 * NT-style WOW32 VDM dispatcher bridge
 *
 * Water normally reaches Win16 code through the Wine relay layer rather than
 * NTVDM.  Native NT WOW, however, routes a packed VDMFRAME through W32Dispatch.
 * This file provides the dispatcher spine while preserving Water's existing
 * generic WOW32 exports.
 */

#include <stddef.h>

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "wine/wow32.h"
#include "wine/vdm.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(wow);

C_ASSERT( offsetof(WINEVDMFRAME, wThunkCSIP) == 28 );
C_ASSERT( offsetof(WINEVDMFRAME, wCallID) == 32 );
C_ASSERT( offsetof(WINEVDMFRAME, cbArgs) == 36 );
C_ASSERT( offsetof(WINEVDMFRAME, vpCSIP) == 38 );
C_ASSERT( offsetof(WINEVDMFRAME, bArgs) == 42 );

#define WOW32_DISPATCH_MAGIC 0x44323357  /* "W32D" */

struct wow32_dispatch_context
{
    DWORD magic;
    WINEVDMFRAME *frame;
    DWORD result;
};

/*
 * Microsoft's WOW thunk table uses __fastcall on x86 (PVDMFRAME in ECX).
 * Water's portable FASTCALL macro intentionally collapses to __stdcall on
 * some non-MinGW GCC/Clang builds, which is not ABI-compatible here.
 */
#if defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
typedef DWORD (__attribute__((fastcall)) *wow32_thunk_proc)(WINEVDMFRAME *);
#else
typedef DWORD (FASTCALL *wow32_thunk_proc)(WINEVDMFRAME *);
#endif

static LONG wow32_initialized;
static LONG wow32_vdm_profile;

typedef BOOL (WINAPI *wow32_dos_int21_proc)(I386_CONTEXT *);
typedef BOOL (__cdecl *wow32_dem_absread_proc)(BYTE, DWORD, DWORD, BYTE *, BOOL);
typedef BOOL (__cdecl *wow32_dem_abswrite_proc)(BYTE, DWORD, DWORD, const BYTE *, BOOL);
typedef void (__cdecl *wow32_dem_exit_proc)(WORD);
static void *wow32_dos_int21_handler;
static void *wow32_dem_absread_handler;
static void *wow32_dem_abswrite_handler;
static void *wow32_dem_exit_handler;

static BOOL wow32_query_region( const void *ptr, SIZE_T size, MEMORY_BASIC_INFORMATION *mbi )
{
    SIZE_T offset;

    if (!ptr || !size) return FALSE;
    if (!VirtualQuery( ptr, mbi, sizeof(*mbi) ) || mbi->State != MEM_COMMIT) return FALSE;
    if (mbi->Protect & (PAGE_GUARD | PAGE_NOACCESS)) return FALSE;
    if ((const BYTE *)ptr < (const BYTE *)mbi->BaseAddress) return FALSE;

    offset = (const BYTE *)ptr - (const BYTE *)mbi->BaseAddress;
    return offset <= mbi->RegionSize && size <= mbi->RegionSize - offset;
}

static BOOL wow32_is_writable( const void *ptr, SIZE_T size )
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD protect;

    if (!wow32_query_region( ptr, size, &mbi )) return FALSE;
    protect = mbi.Protect & 0xff;

    return protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
           protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

static BOOL wow32_is_executable( const void *ptr )
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD protect;

    if (!wow32_query_region( ptr, 1, &mbi )) return FALSE;
    protect = mbi.Protect & 0xff;

    return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
           protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

/***********************************************************************
 *           W32Init
 *
 * NT5 exports this with one argument (fMEoW).  Water does not need NTVDM's
 * duplicated USER/GDI registration because its Win16 modules already share
 * the Wine process, but the dispatcher still needs an explicit initialized
 * state for callers that follow the NT startup contract.
 */
BOOL WINAPI W32Init( BOOL fMEoW )
{
    TRACE( "(%u)\n", fMEoW );
    InterlockedExchange( &wow32_initialized, TRUE );
    return TRUE;
}

/***********************************************************************
 *           __wine_W32RegisterVdmProfile
 *
 * NTVDM owns NT WOW personality selection. WOW32 only transports the
 * process-local scalar profile to Win16 consumers loaded later.
 */
void __cdecl __wine_W32RegisterVdmProfile( DWORD profile )
{
    switch (profile)
    {
    case WATER_VDM_WOW_PROFILE_NT31:
    case WATER_VDM_WOW_PROFILE_NT351:
    case WATER_VDM_WOW_PROFILE_NT5:
        break;
    default:
        WARN( "invalid NTVDM WOW profile %#lx\n", profile );
        profile = WATER_VDM_WOW_PROFILE_NONE;
        break;
    }

    InterlockedExchange( &wow32_vdm_profile, profile );
}

DWORD __cdecl __wine_W32GetVdmProfile( void )
{
    return InterlockedCompareExchange( &wow32_vdm_profile, 0, 0 );
}

/***********************************************************************
 *           __wine_W32RegisterDosInt21Handler
 *
 * NTVDM owns the NT-side DOS emulation state.  Register the process-local
 * INT 21h service hook before KRNL386 starts running Win16 tasks.
 */
void __cdecl __wine_W32RegisterDosInt21Handler( void *handler )
{
    InterlockedExchangePointer( &wow32_dos_int21_handler, handler );
}

/***********************************************************************
 *           __wine_W32DosInt21
 *
 * Return TRUE only when NTVDM handled the DOS request.  KRNL386 retains its
 * existing handler for Win16 task/PSP/vector services and as a fallback.
 */
BOOL __cdecl __wine_W32DosInt21( I386_CONTEXT *context )
{
    wow32_dos_int21_proc proc =
        (wow32_dos_int21_proc)InterlockedCompareExchangePointer( &wow32_dos_int21_handler, NULL, NULL );

    return proc ? proc( context ) : FALSE;
}

/***********************************************************************
 *           __wine_W32RegisterDemHandlers
 *
 * NTVDM owns host-side DOS emulation manager services.  Keep those callbacks
 * process-local and let KRNL386/NTDOS compatibility code cross the WOW32
 * boundary instead of linking NTVDM implementation objects into KRNL386.
 */
void __cdecl __wine_W32RegisterDemHandlers( void *read_handler, void *write_handler,
                                            void *exit_handler )
{
    InterlockedExchangePointer( &wow32_dem_absread_handler, read_handler );
    InterlockedExchangePointer( &wow32_dem_abswrite_handler, write_handler );
    InterlockedExchangePointer( &wow32_dem_exit_handler, exit_handler );
}

BOOL __cdecl __wine_W32DemAbsoluteRead( BYTE drive, DWORD begin, DWORD nr_sect,
                                        BYTE *dataptr, BOOL fake_success )
{
    wow32_dem_absread_proc proc =
        (wow32_dem_absread_proc)InterlockedCompareExchangePointer( &wow32_dem_absread_handler,
                                                                   NULL, NULL );

    return proc ? proc( drive, begin, nr_sect, dataptr, fake_success ) : FALSE;
}

BOOL __cdecl __wine_W32DemAbsoluteWrite( BYTE drive, DWORD begin, DWORD nr_sect,
                                         const BYTE *dataptr, BOOL fake_success )
{
    wow32_dem_abswrite_proc proc =
        (wow32_dem_abswrite_proc)InterlockedCompareExchangePointer( &wow32_dem_abswrite_handler,
                                                                    NULL, NULL );

    return proc ? proc( drive, begin, nr_sect, dataptr, fake_success ) : FALSE;
}

BOOL __cdecl __wine_W32DemExitTask( WORD retval )
{
    wow32_dem_exit_proc proc =
        (wow32_dem_exit_proc)InterlockedCompareExchangePointer( &wow32_dem_exit_handler,
                                                                NULL, NULL );

    if (!proc) return FALSE;
    proc( retval );
    return TRUE;
}

static DWORD wow32_dispatch_frame( WINEVDMFRAME *frame )
{
    wow32_thunk_proc proc;
    DWORD call_id, ret;
    SIZE_T frame_size;

    if (!frame) return 0;

    frame_size = offsetof(WINEVDMFRAME, bArgs) + frame->cbArgs;
    if (!wow32_is_writable( frame, frame_size ))
    {
        WARN( "invalid VDM frame %p size %Iu\n", frame, frame_size );
        return 0;
    }

    if (!wow32_initialized) W32Init( FALSE );

    call_id = frame->wCallID;

    /*
     * Native W32Dispatch accepts both a numeric thunk-table id and, after
     * patching, the thunk procedure address itself.  Water does not yet have
     * the NT wktbl/wutbl/wgtbl tables, so only the patched-address path is
     * dispatchable here.  Reject unresolved ids instead of treating them as
     * pointers and jumping into low memory.
     */
    if (!HIWORD( call_id ))
    {
        FIXME( "unresolved NT WOW thunk id %#lx\n", call_id );
        return 0;
    }

    /*
     * VDMFRAME stores a 32-bit thunk address.  A native WOW32 dispatcher is
     * therefore meaningful only in a 32-bit WOW module.
     */
    if (sizeof(void *) > sizeof(call_id))
    {
        WARN( "cannot dispatch 32-bit WOW thunk address %#lx from a %u-bit module\n",
              call_id, (unsigned int)(8 * sizeof(void *)) );
        return 0;
    }

    proc = (wow32_thunk_proc)(ULONG_PTR)call_id;
    if (!wow32_is_executable( (const void *)proc ))
    {
        WARN( "invalid WOW thunk address %p\n", proc );
        return 0;
    }

    ret = proc( frame );
    frame->wAX = LOWORD( ret );
    frame->wDX = HIWORD( ret );
    return ret;
}

/***********************************************************************
 *           W32Dispatch
 *
 * The public NT entry point has no parameters.  Water's NT5 KERNEL.506 bridge
 * installs a per-call context in the TEB WOW32Reserved slot, invokes this
 * function, and restores the previous value afterwards.
 */
void WINAPI W32Dispatch( void )
{
    struct wow32_dispatch_context *context = NtCurrentTeb()->WOW32Reserved;
    MEMORY_BASIC_INFORMATION mbi;

    if (!wow32_query_region( context, sizeof(*context), &mbi ) ||
        context->magic != WOW32_DISPATCH_MAGIC || !context->frame)
    {
        WARN( "called without a Water WOW VDM dispatch context\n" );
        return;
    }

    context->result = wow32_dispatch_frame( context->frame );
}

/***********************************************************************
 *           __wine_W32DispatchFrame
 *
 * Private bridge used by krnl386.exe16's WOW16Call implementation.
 */
DWORD __cdecl __wine_W32DispatchFrame( WINEVDMFRAME *frame )
{
    struct wow32_dispatch_context context;
    void *previous = NtCurrentTeb()->WOW32Reserved;

    context.magic = WOW32_DISPATCH_MAGIC;
    context.frame = frame;
    context.result = 0;

    NtCurrentTeb()->WOW32Reserved = &context;
    W32Dispatch();
    NtCurrentTeb()->WOW32Reserved = previous;

    return context.result;
}


/*
 * WOWDEB remote-helper state.
 *
 * The segmented block address is process-local.  VDMDBG's cross-process
 * transport will eventually consume this state through the target process,
 * matching NT's W32RemoteThread/DBGNotifyRemoteThreadAddress design.
 */
static LONG wowdeb_remote_block;

/***********************************************************************
 *           __wine_WOWDebugPoll16
 *
 * Private bridge used by KERNEL.511.  At present it registers the live
 * WOWDEB communication block and reports that no remote request is pending.
 */
BOOL __cdecl __wine_WOWDebugPoll16( DWORD block )
{
    DWORD previous;

    previous = InterlockedExchange( &wowdeb_remote_block, block );
    if (previous != block)
    {
        if (block) TRACE( "WOWDEB remote block registered at %08lx\n", block );
        else if (previous) TRACE( "WOWDEB remote block %08lx unregistered\n", previous );
    }

    return FALSE;
}

/***********************************************************************
 *           __wine_WOWDebugGetRemoteBlock
 *
 * Private inspection hook for the VDMDBG transport and tests.
 */
DWORD __cdecl __wine_WOWDebugGetRemoteBlock( void )
{
    return InterlockedCompareExchange( &wowdeb_remote_block, 0, 0 );
}
