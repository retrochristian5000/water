/*
 * x86-64 emulation on ARM64
 *
 * Copyright 2024 Alexandre Julliard
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

#include "ntstatus.h"
#include "windef.h"
#include "winbase.h"
#include "winnt.h"
#include "winternl.h"
#include "unixlib.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xtajit);

static BOOL unix_ready;


/**********************************************************************
 *           DispatchJump  (xtajit64.@)
 *
 * Implementation of __os_arm64x_x64_jump.
 */
void WINAPI DispatchJump(void)
{
    ERR( "x64 emulation not implemented\n" );
    NtTerminateProcess( GetCurrentProcess(), 1 );
}


/**********************************************************************
 *           RetToEntryThunk  (xtajit64.@)
 *
 * Implementation of __os_arm64x_dispatch_ret.
 */
void WINAPI RetToEntryThunk(void)
{
    ERR( "x64 emulation not implemented\n" );
    NtTerminateProcess( GetCurrentProcess(), 1 );
}


/**********************************************************************
 *           ExitToX64  (xtajit64.@)
 *
 * Implementation of __os_arm64x_dispatch_call_no_redirect.
 */
void WINAPI ExitToX64(void)
{
    ERR( "x64 emulation not implemented\n" );
    NtTerminateProcess( GetCurrentProcess(), 1 );
}


/**********************************************************************
 *           BeginSimulation  (xtajit64.@)
 */
void WINAPI BeginSimulation(void)
{
    ERR( "x64 emulation not implemented\n" );
    NtTerminateProcess( GetCurrentProcess(), 1 );
}


/**********************************************************************
 *           BTCpu64FlushInstructionCache  (xtajit64.@)
 */
void WINAPI BTCpu64FlushInstructionCache( void *addr, SIZE_T size )
{
    struct xtajit_addr_size_params params = { (UINT_PTR)addr, size };

    TRACE( "%p %Ix\n", addr, size );
    if (unix_ready) XTAJIT_CALL( flush_instruction_cache, &params );
}


/**********************************************************************
 *           BTCpu64IsProcessorFeaturePresent  (xtajit64.@)
 */
BOOLEAN WINAPI BTCpu64IsProcessorFeaturePresent( UINT feature )
{
    static const ULONGLONG x86_features =
        (1ull << PF_COMPARE_EXCHANGE_DOUBLE) |
        (1ull << PF_MMX_INSTRUCTIONS_AVAILABLE) |
        (1ull << PF_XMMI_INSTRUCTIONS_AVAILABLE) |
        (1ull << PF_RDTSC_INSTRUCTION_AVAILABLE) |
        (1ull << PF_XMMI64_INSTRUCTIONS_AVAILABLE) |
        (1ull << PF_NX_ENABLED) |
        (1ull << PF_SSE3_INSTRUCTIONS_AVAILABLE) |
        (1ull << PF_COMPARE_EXCHANGE128) |
        (1ull << PF_FASTFAIL_AVAILABLE) |
        (1ull << PF_RDTSCP_INSTRUCTION_AVAILABLE) |
        (1ull << PF_SSSE3_INSTRUCTIONS_AVAILABLE) |
        (1ull << PF_SSE4_1_INSTRUCTIONS_AVAILABLE) |
        (1ull << PF_SSE4_2_INSTRUCTIONS_AVAILABLE);

    return feature < 64 && (x86_features & (1ull << feature));
}


/**********************************************************************
 *           BTCpu64NotifyMemoryDirty  (xtajit64.@)
 */
void WINAPI BTCpu64NotifyMemoryDirty( void *addr, SIZE_T size )
{
    struct xtajit_addr_size_params params = { (UINT_PTR)addr, size };

    TRACE( "%p %Ix\n", addr, size );
    if (unix_ready) XTAJIT_CALL( notify_memory_dirty, &params );
}


/**********************************************************************
 *           BTCpu64NotifyReadFile  (xtajit64.@)
 */
void WINAPI BTCpu64NotifyReadFile( HANDLE handle, void *addr, SIZE_T size, BOOL is_post, NTSTATUS status )
{
    struct xtajit_read_file_params params = { (UINT_PTR)handle, (UINT_PTR)addr, size, is_post, status };

    TRACE( "%p %p %Ix\n", handle, addr, size );
    if (unix_ready) XTAJIT_CALL( notify_read_file, &params );
}


/**********************************************************************
 *           FlushInstructionCacheHeavy  (xtajit64.@)
 */
void WINAPI FlushInstructionCacheHeavy( void *addr, SIZE_T size )
{
    struct xtajit_addr_size_params params = { (UINT_PTR)addr, size };

    TRACE( "%p %Ix\n", addr, size );
    if (unix_ready) XTAJIT_CALL( flush_instruction_cache_heavy, &params );
}


/**********************************************************************
 *           NotifyMapViewOfSection  (xtajit64.@)
 */
NTSTATUS WINAPI NotifyMapViewOfSection( void *unk1, void *addr, void *unk2, SIZE_T size,
                                        ULONG alloc_type, ULONG protect )
{
    struct xtajit_map_view_params params =
    {
        (UINT_PTR)unk1, (UINT_PTR)addr, (UINT_PTR)unk2, size, alloc_type, protect
    };

    TRACE( "%p %Ix %lx %lx\n", addr, size, alloc_type, protect );
    return unix_ready ? XTAJIT_CALL( notify_map_view, &params ) : STATUS_SUCCESS;
}


/**********************************************************************
 *           NotifyMemoryAlloc  (xtajit64.@)
 */
void WINAPI NotifyMemoryAlloc( void *addr, SIZE_T size, ULONG type, ULONG prot, BOOL is_post, NTSTATUS status )
{
    struct xtajit_memory_params params = { (UINT_PTR)addr, size, type, prot, is_post, status };

    TRACE( "%p %Ix\n", addr, size );
    if (unix_ready) XTAJIT_CALL( notify_memory_alloc, &params );
}


/**********************************************************************
 *           NotifyMemoryFree  (xtajit64.@)
 */
void WINAPI NotifyMemoryFree( void *addr, SIZE_T size, ULONG type, BOOL is_post, NTSTATUS status )
{
    struct xtajit_memory_params params = { (UINT_PTR)addr, size, type, 0, is_post, status };

    TRACE( "%p %Ix %lx\n", addr, size, type );
    if (unix_ready) XTAJIT_CALL( notify_memory_free, &params );
}


/**********************************************************************
 *           NotifyMemoryProtect  (xtajit64.@)
 */
void WINAPI NotifyMemoryProtect( void *addr, SIZE_T size, ULONG prot, BOOL is_post, NTSTATUS status )
{
    struct xtajit_memory_params params = { (UINT_PTR)addr, size, 0, prot, is_post, status };

    TRACE( "%p %Ix %lx\n", addr, size, prot );
    if (unix_ready) XTAJIT_CALL( notify_memory_protect, &params );
}


/**********************************************************************
 *           NotifyUnmapViewOfSection  (xtajit64.@)
 */
void WINAPI NotifyUnmapViewOfSection( void *addr, BOOL is_post, NTSTATUS status )
{
    struct xtajit_unmap_view_params params = { (UINT_PTR)addr, is_post, status };

    TRACE( "%p\n", addr );
    if (unix_ready) XTAJIT_CALL( notify_unmap_view, &params );
}


/**********************************************************************
 *           ProcessInit  (xtajit64.@)
 */
NTSTATUS WINAPI ProcessInit(void)
{
    NTSTATUS status;

    if ((status = __wine_init_unix_call()))
    {
        TRACE( "No xtajit64 Unix backend available, keeping stub behavior: %#lx\n", status );
        return STATUS_SUCCESS;
    }

    status = XTAJIT_CALL( process_init, NULL );
    if (NT_SUCCESS(status)) unix_ready = TRUE;
    return status;
}


/**********************************************************************
 *           ProcessTerm  (xtajit64.@)
 */
void WINAPI ProcessTerm( HANDLE handle, BOOL is_post, NTSTATUS status )
{
    struct xtajit_process_term_params params = { (UINT_PTR)handle, is_post, status };

    TRACE( "%p\n", handle );
    if (unix_ready) XTAJIT_CALL( process_term, &params );
}


/**********************************************************************
 *           ResetToConsistentState  (xtajit64.@)
 */
void WINAPI ResetToConsistentState( EXCEPTION_RECORD *rec, CONTEXT *context, ARM64_NT_CONTEXT *arm_ctx )
{
    struct xtajit_reset_params params = { (UINT_PTR)rec, (UINT_PTR)context, (UINT_PTR)arm_ctx };

    TRACE( "%p %p %p\n", rec, context, arm_ctx );
    if (unix_ready) XTAJIT_CALL( reset_to_consistent_state, &params );
}


/**********************************************************************
 *           ThreadInit  (xtajit64.@)
 */
NTSTATUS WINAPI ThreadInit(void)
{
    return unix_ready ? XTAJIT_CALL( thread_init, NULL ) : STATUS_SUCCESS;
}


/**********************************************************************
 *           ThreadTerm  (xtajit64.@)
 */
void WINAPI ThreadTerm( HANDLE handle, LONG exit_code )
{
    struct xtajit_thread_term_params params = { (UINT_PTR)handle, exit_code };

    TRACE( "%p %lx\n", handle, exit_code );
    if (unix_ready) XTAJIT_CALL( thread_term, &params );
}


/**********************************************************************
 *           UpdateProcessorInformation  (xtajit64.@)
 */
void WINAPI UpdateProcessorInformation( SYSTEM_CPU_INFORMATION *info )
{
    info->ProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
    info->ProcessorLevel = 21;
    info->ProcessorRevision = 1;
}


/**********************************************************************
 *           DllMain
 */
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    if (reason == DLL_PROCESS_ATTACH) LdrDisableThreadCalloutsForDll( inst );
    return TRUE;
}
