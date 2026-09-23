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

#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "ntstatus.h"
#include "windef.h"
#include "winnt.h"
#include "winternl.h"
#include "unixlib.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(xtajit);

struct uc_engine;

struct unicorn_api
{
    void *module;
    unsigned int (*version)( unsigned int *major, unsigned int *minor );
    int (*open)( int arch, int mode, struct uc_engine **engine );
    int (*close)( struct uc_engine *engine );
    const char *(*strerror)( int error );
};

static struct unicorn_api unicorn;
static pthread_key_t unicorn_engine_key;
static BOOL unicorn_key_valid;
static BOOL unicorn_required;

enum
{
    UNICORN_ARCH_X86 = 4,
    UNICORN_MODE_64 = 1 << 3,
    UNICORN_ERR_OK = 0,
    UNICORN_API_MAJOR = 2,
};

static void close_unicorn_module(void)
{
    if (unicorn.module) dlclose( unicorn.module );
    memset( &unicorn, 0, sizeof(unicorn) );
}

static BOOL load_unicorn_symbol( void **func, const char *name )
{
    if ((*func = dlsym( unicorn.module, name ))) return TRUE;
    WARN( "Unicorn is missing required symbol %s\n", name );
    return FALSE;
}

static BOOL load_unicorn(void)
{
    static const char * const default_names[] =
    {
#ifdef __APPLE__
        "libunicorn.2.dylib",
        "libunicorn.dylib",
#else
        "libunicorn.so.2",
        "libunicorn.so",
#endif
        NULL
    };
    const char *backend = getenv( "WINE_XTAJIT_BACKEND" );
    const char *library = getenv( "WINE_UNICORN_LIBRARY" );
    unsigned int major = 0, minor = 0;
    unsigned int i;

    unicorn_required = backend && !strcmp( backend, "unicorn" );
    if (backend && !strcmp( backend, "none" ))
    {
        TRACE( "Unicorn backend disabled by WINE_XTAJIT_BACKEND\n" );
        return FALSE;
    }
    if (backend && strcmp( backend, "auto" ) && strcmp( backend, "unicorn" ))
    {
        WARN( "Unknown WINE_XTAJIT_BACKEND value %s\n", backend );
        return FALSE;
    }

    if (library && *library)
    {
        TRACE( "Trying Unicorn library %s\n", library );
        unicorn.module = dlopen( library, RTLD_NOW | RTLD_LOCAL );
    }
    else
    {
        for (i = 0; default_names[i]; i++)
        {
            TRACE( "Trying Unicorn library %s\n", default_names[i] );
            if ((unicorn.module = dlopen( default_names[i], RTLD_NOW | RTLD_LOCAL ))) break;
        }
    }

    if (!unicorn.module)
    {
        TRACE( "Unicorn backend unavailable: %s\n", dlerror() );
        return FALSE;
    }

#define LOAD_UNICORN_FUNC(name)     if (!load_unicorn_symbol( (void **)&unicorn.name, "uc_" #name ))     {         close_unicorn_module();         return FALSE;     }

    LOAD_UNICORN_FUNC( version );
    LOAD_UNICORN_FUNC( open );
    LOAD_UNICORN_FUNC( close );
    LOAD_UNICORN_FUNC( strerror );
#undef LOAD_UNICORN_FUNC

    unicorn.version( &major, &minor );
    if (major != UNICORN_API_MAJOR)
    {
        WARN( "Unsupported Unicorn API version %u.%u, expected major %u\n",
              major, minor, UNICORN_API_MAJOR );
        close_unicorn_module();
        return FALSE;
    }

    TRACE( "Loaded Unicorn API %u.%u\n", major, minor );
    return TRUE;
}

static struct uc_engine *get_unicorn_engine(void)
{
    return unicorn_key_valid ? pthread_getspecific( unicorn_engine_key ) : NULL;
}

static void close_unicorn_engine(void)
{
    struct uc_engine *engine = get_unicorn_engine();

    if (!engine) return;
    pthread_setspecific( unicorn_engine_key, NULL );
    unicorn.close( engine );
}

static NTSTATUS xtajit_process_init( void *args )
{
    (void)args;

    if (!load_unicorn())
        return unicorn_required ? STATUS_NOT_SUPPORTED : STATUS_SUCCESS;

    if (pthread_key_create( &unicorn_engine_key, NULL ))
    {
        close_unicorn_module();
        return unicorn_required ? STATUS_NO_MEMORY : STATUS_SUCCESS;
    }
    unicorn_key_valid = TRUE;
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_process_term( void *args )
{
    (void)args;

    close_unicorn_engine();
    if (unicorn_key_valid)
    {
        pthread_key_delete( unicorn_engine_key );
        unicorn_key_valid = FALSE;
    }
    close_unicorn_module();
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_thread_init( void *args )
{
    struct uc_engine *engine;
    int error;

    (void)args;
    if (!unicorn.module || !unicorn_key_valid || get_unicorn_engine()) return STATUS_SUCCESS;

    if ((error = unicorn.open( UNICORN_ARCH_X86, UNICORN_MODE_64, &engine )) != UNICORN_ERR_OK)
    {
        WARN( "Failed to create Unicorn x86-64 engine: %s\n", unicorn.strerror( error ) );
        return unicorn_required ? STATUS_NOT_SUPPORTED : STATUS_SUCCESS;
    }
    if (pthread_setspecific( unicorn_engine_key, engine ))
    {
        unicorn.close( engine );
        return unicorn_required ? STATUS_NO_MEMORY : STATUS_SUCCESS;
    }

    TRACE( "Created Unicorn x86-64 engine %p\n", engine );
    return STATUS_SUCCESS;
}

static NTSTATUS xtajit_thread_term( void *args )
{
    (void)args;
    close_unicorn_engine();
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
