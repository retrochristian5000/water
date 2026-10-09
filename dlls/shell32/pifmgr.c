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
    BYTE *original;
    WCHAR *path;
    DWORD size;
    BOOL dirty;
};

static BOOL load_pif_file(struct pifmgr_handle *pif, const WCHAR *path)
{
    LARGE_INTEGER size;
    DWORD file_size, read;
    HANDLE file;
    BYTE *data, *original = NULL;
    WCHAR *saved_path;
    size_t name_chars;

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
    name_chars = lstrlenW(path) + 1;
    saved_path = malloc(name_chars * sizeof(*saved_path));
    if (file_size) original = malloc(file_size);
    if (!saved_path || (file_size && !original))
    {
        free(saved_path);
        free(original);
        free(data);
        return FALSE;
    }
    memcpy(saved_path, path, name_chars * sizeof(*saved_path));
    if (file_size) memcpy(original, data, file_size);
    free(pif->data);
    free(pif->original);
    free(pif->path);
    pif->data = data;
    pif->original = original;
    pif->path = saved_path;
    pif->size = file_size;
    pif->dirty = FALSE;
    return TRUE;
}

/* Preserve the original bytes for conflict detection, and never truncate a
 * file merely because its PIF extension data has not been fully decoded. */
static BOOL save_pif_file(const struct pifmgr_handle *pif)
{
    HANDLE file;
    LARGE_INTEGER length;
    BYTE *disk;
    DWORD read, written;
    BOOL ok = FALSE;

    if (!pif->path || !pif->original || !pif->size) return FALSE;
    file = CreateFileW(pif->path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
            NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;

    if (!(disk = malloc(pif->size))) goto done;
    if (!GetFileSizeEx(file, &length) || length.QuadPart != pif->size ||
        !ReadFile(file, disk, pif->size, &read, NULL) || read != pif->size ||
        memcmp(disk, pif->original, pif->size))
        goto release;

    if (SetFilePointer(file, 0, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER)
        goto release;
    if (!WriteFile(file, pif->data, pif->size, &written, NULL) || written != pif->size)
        goto release;
    ok = FlushFileBuffers(file);

release:
    free(disk);
done:
    CloseHandle(file);
    return ok;
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

    /* Old 0x171-byte TopView PIFs have no Microsoft extension chain.
     * Do not interpret an arbitrary file as a linked PIF structure. */
    if (!pif->data || pif->size < PIF_BASE_SIZE + sizeof(*record) ||
        memcmp(pif->data + PIF_BASE_SIZE, "MICROSOFT PIFEX", 16))
        return FALSE;

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
 * PifMgr_SetProperties [SHELL32.11]
 *
 * Only existing fixed-size named blocks are writable. Do not change
 * offsets or fabricate new extensions without a verified format writer.
 */
int WINAPI PifMgr_SetProperties(HANDLE handle, LPCSTR group, const void *buffer, int size, UINT flags)
{
    struct pifmgr_handle *pif = handle;
    struct pif_record_header record;

    TRACE("handle %p, group %s, buffer %p, size %d, flags %#x.\n",
            handle, debugstr_a(group), buffer, size, flags);

    if (!pif || pif->magic != PIFMGR_MAGIC || !pif->data || !pif->path ||
        flags != SETPROPS_NONE || !group || !((ULONG_PTR)group >> 16) ||
        !buffer || size <= 0 || size > 0xffff)
        return 0;

    if (!get_record(pif, 0, group, &record) || size != record.size)
        return 0;

    memmove(pif->data + record.data, buffer, size);
    pif->dirty = TRUE;
    return size;
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
    {
        FIXME("unsupported flags %#x.\n", flags);
        return handle;
    }
    if (pif->dirty && !(flags & CLOSEPROPS_DISCARD) && !save_pif_file(pif))
    {
        WARN("could not save PIF %s; preserving open properties.\n",
             debugstr_w(pif->path));
        return handle;
    }

    pif->magic = 0;
    free(pif->data);
    free(pif->original);
    free(pif->path);
    free(pif);
    return NULL;
}
