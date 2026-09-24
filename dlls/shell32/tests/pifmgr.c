/*
 * Program Information File manager tests
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

#include <string.h>

#include "windef.h"
#include "winbase.h"

#include "wine/test.h"

#define PIF_BASE_SIZE 0x171

#pragma pack(push,1)
struct pif_record_header
{
    char name[16];
    WORD next;
    WORD data;
    WORD size;
};
#pragma pack(pop)

static HANDLE (WINAPI *pPifMgr_OpenProperties)(LPCWSTR, LPCWSTR, UINT, UINT);
static int (WINAPI *pPifMgr_GetProperties)(HANDLE, LPCSTR, void *, int, UINT);
static HANDLE (WINAPI *pPifMgr_CloseProperties)(HANDLE, UINT);

static void write_record(BYTE *data, WORD offset, const char *name, WORD next, WORD data_offset, WORD size)
{
    struct pif_record_header record = {0};

    lstrcpynA(record.name, name, sizeof(record.name));
    record.next = next;
    record.data = data_offset;
    record.size = size;
    memcpy(data + offset, &record, sizeof(record));
}

static BOOL create_test_pif(const WCHAR *path)
{
    static const BYTE vmm_data[] = {1, 2, 3, 4};
    static const char config_data[] = "DOS=HIGH";
    enum
    {
        pifex_offset = PIF_BASE_SIZE,
        vmm_offset = pifex_offset + sizeof(struct pif_record_header),
        vmm_data_offset = vmm_offset + sizeof(struct pif_record_header),
        config_offset = vmm_data_offset + sizeof(vmm_data),
        config_data_offset = config_offset + sizeof(struct pif_record_header),
        file_size = config_data_offset + sizeof(config_data) - 1
    };
    BYTE data[file_size];
    HANDLE file;
    DWORD written;

    memset(data, 0, sizeof(data));
    write_record(data, pifex_offset, "MICROSOFT PIFEX", vmm_offset, 0, PIF_BASE_SIZE);
    write_record(data, vmm_offset, "WINDOWS VMM 4.0", config_offset,
            vmm_data_offset, (WORD)sizeof(vmm_data));
    write_record(data, config_offset, "CONFIG SYS 4.0", 0xffff,
            config_data_offset, (WORD)(sizeof(config_data) - 1));
    memcpy(data + vmm_data_offset, vmm_data, sizeof(vmm_data));
    memcpy(data + config_data_offset, config_data, sizeof(config_data) - 1);

    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return FALSE;
    if (!WriteFile(file, data, (DWORD)sizeof(data), &written, NULL) || written != (DWORD)sizeof(data))
    {
        CloseHandle(file);
        return FALSE;
    }
    CloseHandle(file);
    return TRUE;
}

static void test_named_groups(void)
{
    WCHAR temp_dir[MAX_PATH], path[MAX_PATH];
    BYTE buffer[16];
    HANDLE pif;
    int ret;

    GetTempPathW(ARRAY_SIZE(temp_dir), temp_dir);
    GetTempFileNameW(temp_dir, L"pif", 0, path);
    ok(create_test_pif(path), "failed to create test PIF.\n");

    pif = pPifMgr_OpenProperties(path, path, ~0u, 0);
    ok(!!pif, "failed to open PIF properties.\n");
    if (!pif) goto done;

    ret = pPifMgr_GetProperties(pif, "WINDOWS VMM 4.0", NULL, 0, 0);
    ok(ret == 4, "got size %d.\n", ret);

    memset(buffer, 0xcc, sizeof(buffer));
    ret = pPifMgr_GetProperties(pif, "WINDOWS VMM 4.0", buffer, 4, 0);
    ok(ret == 4, "got %d bytes.\n", ret);
    ok(buffer[0] == 1 && buffer[1] == 2 && buffer[2] == 3 && buffer[3] == 4,
            "unexpected VMM data.\n");

    memset(buffer, 0, sizeof(buffer));
    ret = pPifMgr_GetProperties(pif, NULL, buffer, 0, 0);
    ok(ret == 16, "got %d bytes.\n", ret);
    ok(!strncmp((char *)buffer, "MICROSOFT PIFEX", 16), "got group %.16s.\n", buffer);

    memset(buffer, 0, sizeof(buffer));
    ret = pPifMgr_GetProperties(pif, NULL, buffer, 1, 0);
    ok(ret == 16, "got %d bytes.\n", ret);
    ok(!strncmp((char *)buffer, "WINDOWS VMM 4.0", 16), "got group %.16s.\n", buffer);

    memset(buffer, 0, sizeof(buffer));
    ret = pPifMgr_GetProperties(pif, NULL, buffer, 2, 0);
    ok(ret == 16, "got %d bytes.\n", ret);
    ok(!strncmp((char *)buffer, "CONFIG SYS 4.0", 16), "got group %.16s.\n", buffer);

    ret = pPifMgr_GetProperties(pif, NULL, buffer, 3, 0);
    ok(!ret, "got %d bytes.\n", ret);

    ret = pPifMgr_GetProperties(pif, "MISSING GROUP", buffer, sizeof(buffer), 0);
    ok(!ret, "got %d bytes.\n", ret);

    ok(!pPifMgr_CloseProperties(pif, 0), "failed to close PIF properties.\n");

done:
    DeleteFileW(path);
}

START_TEST(pifmgr)
{
    HMODULE shell32 = GetModuleHandleW(L"shell32.dll");

    pPifMgr_OpenProperties = (void *)GetProcAddress(shell32, (const char *)(ULONG_PTR)9);
    pPifMgr_GetProperties = (void *)GetProcAddress(shell32, (const char *)(ULONG_PTR)10);
    pPifMgr_CloseProperties = (void *)GetProcAddress(shell32, (const char *)(ULONG_PTR)13);

    if (!pPifMgr_OpenProperties || !pPifMgr_GetProperties || !pPifMgr_CloseProperties)
    {
        win_skip("PIF manager exports are not available.\n");
        return;
    }

    test_named_groups();
}
