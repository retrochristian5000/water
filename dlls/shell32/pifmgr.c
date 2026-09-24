/*
 * Program Information File manager
 *
 * Copyright 2026
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

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "windef.h"
#include "winbase.h"
#include "shlobj.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(shell);

#define PIF_BASE_SIZE 0x171
#define PIFMGR_MAGIC 0x5049464d

#pragma pack(push,1)
struct pif_record_header
{
    char name[16];
    WORD next;
    WORD data;
    WORD size;
};
#pragma pack(pop)

struct pifmgr_handle
{
    DWORD magic;
    BYTE *data;
    DWORD size;
};

static BOOL load_pif_file(struct pifmgr_handle *pif, const WCHAR *path)
{
    LARGE_INTEGER size;
    DWORD file_size, read;
    HANDLE file;
    BYTE *data;

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    if (!GetFileSizeEx(file, &size) || size.QuadPart > 0xffff)
    {
        CloseHandle(file);
        return FALSE;
    }

    file_size = size.LowPart;
    if (file_size)
    {
        if (!(data = malloc(file_size)))
        {
            CloseHandle(file);
            return FALSE;
        }

        if (!ReadFile(file, data, file_size, &read, NULL) || read != file_size)
        {
            free(data);
            CloseHandle(file);
            return FALSE;
        }
    }
    else data = NULL;

    CloseHandle(file);
    free(pif->data);
    pif->data = data;
    pif->size = file_size;
    return TRUE;
}

static BOOL try_load_app_pif(struct pifmgr_handle *pif, const WCHAR *app)
{
    WCHAR path[MAX_PATH], win_dir[MAX_PATH], *name, *dot, *slash;
    DWORD len;

    if (!app || !*app) return FALSE;

    lstrcpynW(path, app, ARRAY_SIZE(path));
    slash = wcsrchr(path, '\\');
    dot = wcsrchr(path, '.');

    if (dot && (!slash || dot > slash))
    {
        if (!lstrcmpiW(dot, L".pif")) return load_pif_file(pif, path);
        *dot = 0;
    }

    if (lstrlenW(path) + 4 < ARRAY_SIZE(path))
    {
        lstrcatW(path, L".pif");
        if (load_pif_file(pif, path)) return TRUE;
    }

    name = wcsrchr(path, '\\');
    name = name ? name + 1 : path;

    if ((len = GetWindowsDirectoryW(win_dir, ARRAY_SIZE(win_dir))) &&
            len < ARRAY_SIZE(win_dir) && len + 5 + lstrlenW(name) < ARRAY_SIZE(win_dir))
    {
        if (win_dir[len - 1] != '\\') lstrcatW(win_dir, L"\\");
        lstrcatW(win_dir, L"PIF\\");
        lstrcatW(win_dir, name);
        if (load_pif_file(pif, win_dir)) return TRUE;
    }

    len = SearchPathW(NULL, name, NULL, ARRAY_SIZE(path), path, NULL);
    if (len && len < ARRAY_SIZE(path))
        return load_pif_file(pif, path);

    return FALSE;
}

static BOOL get_record(const struct pifmgr_handle *pif, unsigned int index, const char *group,
        struct pif_record_header *record)
{
    DWORD offset = PIF_BASE_SIZE;
    unsigned int current = 0;

    while (offset + sizeof(*record) <= pif->size)
    {
        char name[17];

        memcpy(record, pif->data + offset, sizeof(*record));
        memcpy(name, record->name, sizeof(record->name));
        name[sizeof(record->name)] = 0;

        if ((DWORD)record->data + record->size > pif->size)
        {
            WARN("invalid PIF group %s at %#lx.\n", debugstr_a(name), offset);
            return FALSE;
        }

        if ((group && !strcmp(name, group)) || (!group && current == index))
            return TRUE;

        if (record->next & 0x8000) break;
        if (record->next <= offset || (DWORD)record->next + sizeof(*record) > pif->size)
        {
            WARN("invalid next PIF group offset %#x.\n", record->next);
            break;
        }

        offset = record->next;
        ++current;
    }

    return FALSE;
}

/*************************************************************************
 * PifMgr_OpenProperties [SHELL32.9]
 */
HANDLE WINAPI PifMgr_OpenProperties(LPCWSTR app, LPCWSTR pif_name, UINT hinf, UINT flags)
{
    struct pifmgr_handle *pif;

    TRACE("app %s, pif %s, hinf %#x, flags %#x.\n",
            debugstr_w(app), debugstr_w(pif_name), hinf, flags);

    if (!(pif = calloc(1, sizeof(*pif)))) return NULL;
    pif->magic = PIFMGR_MAGIC;

    if (hinf && hinf != ~0u)
        FIXME("INF processing is not implemented.\n");

    if (flags & ~OPENPROPS_INHIBITPIF)
        FIXME("unsupported flags %#x.\n", flags);

    if (!(flags & OPENPROPS_INHIBITPIF))
    {
        if (pif_name && *pif_name)
        {
            if (!load_pif_file(pif, pif_name))
                TRACE("PIF file %s was not found.\n", debugstr_w(pif_name));
        }
        else if (!try_load_app_pif(pif, app))
            TRACE("no PIF file found for %s.\n", debugstr_w(app));
    }

    return pif;
}

/*************************************************************************
 * PifMgr_GetProperties [SHELL32.10]
 */
int WINAPI PifMgr_GetProperties(HANDLE handle, LPCSTR group, void *buffer, int size, UINT flags)
{
    struct pifmgr_handle *pif = handle;
    struct pif_record_header record;
    int copy_size;

    TRACE("handle %p, group %s, buffer %p, size %d, flags %#x.\n",
            handle, debugstr_a(group), buffer, size, flags);

    if (!pif || pif->magic != PIFMGR_MAGIC || !pif->data) return 0;
    if (flags != GETPROPS_NONE)
    {
        FIXME("unsupported flags %#x.\n", flags);
        return 0;
    }

    if (!group)
    {
        if (size < 0 || !buffer || !get_record(pif, size, NULL, &record)) return 0;
        memcpy(buffer, record.name, sizeof(record.name));
        return sizeof(record.name);
    }

    if (!((ULONG_PTR)group >> 16))
    {
        FIXME("ordinal property group %u is not implemented.\n", LOWORD((ULONG_PTR)group));
        return 0;
    }

    if (!get_record(pif, 0, group, &record)) return 0;
    if (!size) return record.size;
    if (size < 0 || !buffer) return 0;

    copy_size = size < record.size ? size : record.size;
    memcpy(buffer, pif->data + record.data, copy_size);
    return copy_size;
}

/*************************************************************************
 * PifMgr_CloseProperties [SHELL32.13]
 */
HANDLE WINAPI PifMgr_CloseProperties(HANDLE handle, UINT flags)
{
    struct pifmgr_handle *pif = handle;

    TRACE("handle %p, flags %#x.\n", handle, flags);

    if (!pif || pif->magic != PIFMGR_MAGIC) return handle;
    if (flags & ~CLOSEPROPS_DISCARD)
        FIXME("unsupported flags %#x.\n", flags);

    pif->magic = 0;
    free(pif->data);
    free(pif);
    return NULL;
}
