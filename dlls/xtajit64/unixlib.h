/*
 * xtajit64 Unix backend ABI
 *
 * Copyright 2026
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#ifndef __WINE_XTAJIT64_UNIXLIB_H
#define __WINE_XTAJIT64_UNIXLIB_H

#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "wine/unixlib.h"

enum xtajit_unix_funcs
{
    unix_process_init,
    unix_process_term,
    unix_thread_init,
    unix_thread_term,
    unix_flush_instruction_cache,
    unix_flush_instruction_cache_heavy,
    unix_notify_memory_dirty,
    unix_notify_read_file,
    unix_notify_map_view,
    unix_notify_memory_alloc,
    unix_notify_memory_free,
    unix_notify_memory_protect,
    unix_notify_unmap_view,
    unix_reset_to_consistent_state,
    unix_funcs_count
};

#define XTAJIT_CALL(func, params) WINE_UNIX_CALL(unix_ ## func, params)

struct xtajit_addr_size_params
{
    UINT64 addr;
    UINT64 size;
};

struct xtajit_read_file_params
{
    UINT64 handle;
    UINT64 addr;
    UINT64 size;
    UINT32 is_post;
    NTSTATUS status;
};

struct xtajit_map_view_params
{
    UINT64 unk1;
    UINT64 addr;
    UINT64 unk2;
    UINT64 size;
    UINT32 alloc_type;
    UINT32 protect;
};

struct xtajit_memory_params
{
    UINT64 addr;
    UINT64 size;
    UINT32 type;
    UINT32 protect;
    UINT32 is_post;
    NTSTATUS status;
};

struct xtajit_unmap_view_params
{
    UINT64 addr;
    UINT32 is_post;
    NTSTATUS status;
};

struct xtajit_process_term_params
{
    UINT64 handle;
    UINT32 is_post;
    NTSTATUS status;
};

struct xtajit_thread_term_params
{
    UINT64 handle;
    LONG exit_code;
};

struct xtajit_reset_params
{
    UINT64 exception_record;
    UINT64 context;
    UINT64 arm_context;
};

C_ASSERT( sizeof(struct xtajit_addr_size_params) == 16 );
C_ASSERT( sizeof(struct xtajit_read_file_params) == 32 );
C_ASSERT( sizeof(struct xtajit_map_view_params) == 40 );
C_ASSERT( sizeof(struct xtajit_memory_params) == 32 );
C_ASSERT( sizeof(struct xtajit_unmap_view_params) == 16 );
C_ASSERT( sizeof(struct xtajit_process_term_params) == 16 );
C_ASSERT( sizeof(struct xtajit_thread_term_params) == 16 );
C_ASSERT( sizeof(struct xtajit_reset_params) == 24 );

#endif /* __WINE_XTAJIT64_UNIXLIB_H */
