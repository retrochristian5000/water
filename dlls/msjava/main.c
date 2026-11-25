/*
 * Microsoft VM compatibility boundary
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 */

#include <stdarg.h>
#include "windef.h"
#include "winbase.h"
#include "objbase.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(msjava);

/* JNI jint and jsize are signed 32-bit values. JavaVM and JNIEnv are
 * opaque here: no invocation table can be returned without a VM. */
#define JNI_OK        0
#define JNI_ERR      -1
#define JNI_EVERSION -3

INT WINAPI JNI_GetCreatedJavaVMs(void **vms, INT capacity, INT *count)
{
    TRACE("(%p %d %p)\n", vms, capacity, count);

    if(!count)
        return JNI_ERR;
    *count = 0;
    if(capacity < 0 || (capacity && !vms))
        return JNI_ERR;

    /* No runtime exists in this process. Do not touch unused slots. */
    return JNI_OK;
}

INT WINAPI JNI_GetDefaultJavaVMInitArgs(void *args)
{
    TRACE("(%p)\n", args);

    if(!args)
        return JNI_ERR;

    /* No JNI version is supported yet. Do not interpret or overwrite
     * legacy JDK 1.1 or later initialization structures. */
    return JNI_EVERSION;
}

INT WINAPI JNI_CreateJavaVM(void **vm, void **env, void *args)
{
    TRACE("(%p %p %p)\n", vm, env, args);

    if(vm) *vm = NULL;
    if(env) *env = NULL;
    if(!vm || !env || !args)
        return JNI_ERR;

    WARN("Microsoft-compatible Java execution backend unavailable.\n");
    return JNI_ERR;
}

HRESULT WINAPI DllCanUnloadNow(void)
{
    return S_OK;
}

HRESULT WINAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void **obj)
{
    TRACE("(%s %s %p)\n", debugstr_guid(clsid), debugstr_guid(iid), obj);

    if(!obj)
        return E_POINTER;
    *obj = NULL;
    return CLASS_E_CLASSNOTAVAILABLE;
}
