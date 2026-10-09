/*
 * W95INF32 Win9x export and limited behavior checks.
 * Source-based ABI evidence: Wine developer proposal (March 2004).
 */

#include "windows.h"
#include "wine/test.h"

START_TEST(w95inf32)
{
    static const char * const exports[] =
    {
        "CtlSetLddPath32@8",
        "GenFormStrWithoutPlaceHolders32@12",
        "GenInstall32@20",
        "GetSETUPXErrorText32@12",
        "w95thk_ThunkData32"
    };
    HMODULE dll;
    unsigned int i;

    if (sizeof(void *) != 4)
    {
        win_skip("W95INF32 is specific to 32-bit Windows 9x\n");
        return;
    }
    dll = LoadLibraryA("w95inf32.dll");
    if (!dll)
    {
        win_skip("W95INF32.DLL is not available\n");
        return;
    }

    for (i = 0; i < ARRAY_SIZE(exports); ++i)
        ok(GetProcAddress(dll, exports[i]) != NULL, "missing export %s\n", exports[i]);

    for (i = 1; i <= ARRAY_SIZE(exports); ++i)
        ok(GetProcAddress(dll, (LPCSTR)(ULONG_PTR)i) != NULL, "missing ordinal %u\n", i);

    {
        typedef DWORD (WINAPI *subst_proc)(LPSTR, LPCSTR, LPCSTR);
        typedef DWORD (WINAPI *install_proc)(LPCSTR, LPCSTR, LPCSTR, DWORD, DWORD);
        typedef BOOL (WINAPI *ldd_proc)(DWORD, LPCSTR);
        subst_proc subst = (subst_proc)GetProcAddress(dll, exports[1]);
        install_proc install = (install_proc)GetProcAddress(dll, exports[2]);
        ldd_proc ldd = (ldd_proc)GetProcAddress(dll, exports[0]);

        if (subst)
            ok(subst(NULL, NULL, NULL) == ERROR_INVALID_PARAMETER,
               "NULL substitution input was not rejected\n");
        if (install)
        {
            ok(install(NULL, NULL, NULL, 0, 0) == ERROR_INVALID_PARAMETER,
               "invalid install inputs were not rejected\n");
            ok(install("missing.inf", "DefaultInstall", NULL, 0x10, 0) ==
               ERROR_CALL_NOT_IMPLEMENTED, "unsupported CFGAUTO flag was accepted\n");
        }
        if (ldd)
        {
            SetLastError(0);
            ok(!ldd(0, "C:\\Windows"), "system LDD unexpectedly succeeded\n");
            ok(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED,
               "system LDD returned error %lu\n", GetLastError());
            SetLastError(0);
            ok(!ldd(0x10000, "C:\\Windows"), "oversized Win16 LDD unexpectedly succeeded\n");
            ok(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED,
               "oversized LDD returned error %lu\n", GetLastError());
        }
    }

    FreeLibrary(dll);
}
