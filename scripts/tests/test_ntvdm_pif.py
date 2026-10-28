#!/usr/bin/env python3
"""Exercise the real NTVDM PIF decoder against legacy and extended files.

The portable shims only replace file I/O.  This is a parser regression test,
not a Windows guest launch or a DOS VM integration test.
Run: python3 scripts/tests/test_ntvdm_pif.py
"""

import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SOURCE_DIR = ROOT / "programs" / "ntvdm"

WINDEF = r"""
#ifndef TEST_WINDEF_H
#define TEST_WINDEF_H
#include <stddef.h>
#include <stdint.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef size_t SIZE_T;
typedef long long LONGLONG;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
#endif
"""

WINBASE = r"""
#ifndef TEST_WINBASE_H
#define TEST_WINBASE_H
#include <stdio.h>
#include "windef.h"
typedef FILE *HANDLE;
typedef struct { long long QuadPart; } LARGE_INTEGER;
#define INVALID_HANDLE_VALUE NULL
#define GENERIC_READ 1
#define FILE_SHARE_READ 1
#define OPEN_EXISTING 3
#define FILE_BEGIN SEEK_SET
#define ERROR_BAD_FORMAT 11
static DWORD last_error;
static void SetLastError(DWORD error) { last_error = error; }
static HANDLE CreateFileA(const char *path, DWORD access, DWORD share,
                          void *security, DWORD create, DWORD flags, HANDLE template_file)
{
    (void)access; (void)share; (void)security; (void)create;
    (void)flags; (void)template_file;
    return fopen(path, "rb");
}
static BOOL GetFileSizeEx(HANDLE file, LARGE_INTEGER *size)
{
    long offset;
    if (fseek(file, 0, SEEK_END) || (offset = ftell(file)) < 0) return FALSE;
    size->QuadPart = offset;
    return !fseek(file, 0, SEEK_SET);
}
static BOOL SetFilePointerEx(HANDLE file, LARGE_INTEGER offset,
                             LARGE_INTEGER *new_offset, DWORD origin)
{
    (void)new_offset;
    return !fseek(file, (long)offset.QuadPart, (int)origin);
}
static BOOL ReadFile(HANDLE file, void *data, DWORD size, DWORD *read, void *overlapped)
{
    (void)overlapped;
    *read = fread(data, 1, size, file);
    return !ferror(file);
}
static BOOL CloseHandle(HANDLE file) { return !fclose(file); }
#endif
"""

DEBUG = r"""
#ifndef TEST_WINE_DEBUG_H
#define TEST_WINE_DEBUG_H
#define WINE_DEFAULT_DEBUG_CHANNEL(name)
#define TRACE(...) do { if (0) fprintf(stderr, __VA_ARGS__); } while (0)
#define debugstr_a(text) (text)
#endif
"""

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "pif.c"

static void put_word(BYTE *data, WORD value)
{
    data[0] = value & 0xff;
    data[1] = value >> 8;
}

static void write_fixture(const char *path, const BYTE *data, size_t length)
{
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fwrite(data, 1, length, file) == length);
    assert(!fclose(file));
}

int main(int argc, char **argv)
{
    enum { base_size = 0x171, record_size = 22, extension_size = 104 };
    BYTE data[base_size + 2 * record_size + extension_size] = {0};
    struct ntvdm_pif pif;
    unsigned int second = base_size + record_size;
    unsigned int payload = second + record_size;
    const char *path;

    assert(argc == 2);
    path = argv[1];
    memcpy(data + 0x24, "APP.COM", 8);
    memcpy(data + 0x02, "Legacy program", 15);
    memcpy(data + 0x65, "C:\\DOS", 7);
    memcpy(data + 0xa5, "-base", 6);
    data[0x63] = 0x12; /* text display and close-on-exit */

    /* Original TopView-style PIF: no Microsoft extension record. */
    write_fixture(path, data, base_size);
    assert(ntvdm_read_pif(path, &pif));
    assert(!strcmp(pif.program, "APP.COM"));
    assert(!strcmp(pif.title, "Legacy program"));
    assert(!strcmp(pif.directory, "C:\\DOS"));
    assert(!strcmp(pif.parameters, "-base"));
    assert(pif.text_mode && pif.close_on_exit);

    /* A truncated base header is not an old-format PIF. */
    write_fixture(path, data, base_size - 1);
    assert(!ntvdm_read_pif(path, &pif));

    /* A partial extension header must not become a valid legacy PIF. */
    write_fixture(path, data, base_size + 1);
    assert(!ntvdm_read_pif(path, &pif));
    assert(last_error == ERROR_BAD_FORMAT);

    /* Valid Microsoft extension chain with a 386-mode parameter override. */
    memcpy(data + base_size, "MICROSOFT PIFEX", 16);
    put_word(data + base_size + 16, second);
    memcpy(data + second, "WINDOWS 386 3.0", 16);
    put_word(data + second + 16, 0xffff);
    put_word(data + second + 18, payload);
    put_word(data + second + 20, extension_size);
    put_word(data + payload + 20, 0x10);
    memcpy(data + payload + 40, "-386", 5);
    write_fixture(path, data, sizeof(data));
    assert(ntvdm_read_pif(path, &pif));
    assert(!strcmp(pif.parameters, "-386"));
    assert(pif.text_mode);

    /* Declared extension length must lie within the file as well. */
    put_word(data + second + 20, extension_size + 1);
    write_fixture(path, data, sizeof(data));
    assert(!ntvdm_read_pif(path, &pif));
    put_word(data + second + 20, extension_size);

    /* Reject arbitrary records instead of treating them as Microsoft PIFs. */
    data[base_size + 15] = 'X';
    write_fixture(path, data, sizeof(data));
    assert(!ntvdm_read_pif(path, &pif));
    puts("NTVDM legacy and extended PIF decoder checks passed");
    return 0;
}
"""


class NtvdmPifTests(unittest.TestCase):
    def test_real_decoder(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("C compiler not installed")
        with tempfile.TemporaryDirectory(prefix="water-ntvdm-pif-") as dirname:
            root = Path(dirname)
            (root / "wine").mkdir()
            (root / "windef.h").write_text(WINDEF, encoding="utf-8")
            (root / "winbase.h").write_text(WINBASE, encoding="utf-8")
            (root / "wine" / "debug.h").write_text(DEBUG, encoding="utf-8")
            (root / "test.c").write_text(HARNESS, encoding="utf-8")
            binary = root / "test_pif"
            build = subprocess.run(
                compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror", "-I", str(root),
                            "-I", str(SOURCE_DIR), str(root / "test.c"), "-o", str(binary)],
                capture_output=True, text=True,
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run([str(binary), str(root / "fixture.pif")],
                                 capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("decoder checks passed", run.stdout)


if __name__ == "__main__":
    unittest.main()
