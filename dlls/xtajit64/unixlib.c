/*
 * xtajit64 native backend shim
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "ntstatus.h"
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "unixlib.h"

static NTSTATUS xtajit_process_init( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_process_term( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_thread_init( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_thread_term( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_flush_instruction_cache( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_flush_instruction_cache_heavy( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_memory_dirty( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_read_file( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_map_view( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_memory_alloc( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_memory_free( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_memory_protect( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_notify_unmap_view( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_reset_to_consistent_state( void *args )
{
    (void)args;
    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    xtajit_process_init,
    xtajit_process_term,
    xtajit_thread_init,
    xtajit_thread_term,
    xtajit_flush_instruction_cache,
    xtajit_flush_instruction_cache_heavy,
    xtajit_notify_memory_dirty,
    xtajit_notify_read_file,
    xtajit_notify_map_view,
    xtajit_notify_memory_alloc,
    xtajit_notify_memory_free,
    xtajit_notify_memory_protect,
    xtajit_notify_unmap_view,
    xtajit_reset_to_consistent_state,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_funcs) == unix_funcs_count );
