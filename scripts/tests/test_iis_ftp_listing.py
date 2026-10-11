#!/usr/bin/env python3
"""Compile the actual WinINet MS-DOS/IIS FTP LIST parser with synthetic listings."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "dlls/wininet/ftp.c"


def extract(source):
    match = re.search(r"static BOOL FTP_ParseDOSListing\s*\([^;]*\)\s*\{", source)
    if not match:
        raise AssertionError("FTP_ParseDOSListing missing")
    depth = 0
    for i in range(match.end() - 1, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():i + 1]
    raise AssertionError("unclosed FTP_ParseDOSListing")


HARNESS = r"""
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef int BOOL;
#define TRUE 1
#define FALSE 0
typedef unsigned int DWORD;
typedef wchar_t *LPWSTR;
typedef struct {
    unsigned short wYear, wMonth, wDayOfWeek, wDay, wHour, wMinute, wSecond, wMilliseconds;
} SYSTEMTIME;
typedef struct {
    BOOL bIsDirectory;
    LPWSTR lpszName;
    DWORD nSize;
    SYSTEMTIME tmLastModified;
    unsigned short permissions;
} FILEPROPERTIESW, *LPFILEPROPERTIESW;
static wchar_t *strdupAtoW(const char *src)
{
    size_t i, length = strlen(src);
    wchar_t *dest = malloc((length + 1) * sizeof(*dest));
    if (!dest) return NULL;
    for (i = 0; i <= length; i++) dest[i] = (unsigned char)src[i];
    return dest;
}
__FUNCTION__
static void check(const char *source, unsigned int year, unsigned int hour,
                  unsigned int directory, unsigned int size, const wchar_t *filename)
{
    FILEPROPERTIESW props = {0};
    if (!FTP_ParseDOSListing(source, &props) ||
        props.tmLastModified.wYear != year || props.tmLastModified.wHour != hour ||
        props.bIsDirectory != directory || props.nSize != size ||
        !props.lpszName || wcscmp(props.lpszName, filename))
    {
        fprintf(stderr, "IIS listing failed: %s\n", source);
        abort();
    }
    free(props.lpszName);
}
int main(void)
{
    FILEPROPERTIESW props = {0};
    const char *bad[] = {
        "", "12-33-20 09:00AM 4 n", "12-01-200 09:00AM 4 n",
        "12-01-20200 09:00AM 4 n", "12-01-08 13:00AM 4 n",
        "12-01-08 09:60AM 4 n", "12-01-08 09:00XM 4 n",
        "12-01-08 09:00AM 4294967296 n", "12-01-08 09:00AM 4",
        "drwxrwxrwx 1 root stuff", "12-01-08 09:00AM <DIR>",
        "12-01-08 09:00AM -3 n", NULL
    };
    unsigned int i;
    check("07-13-03  08:55PM       <DIR>          My Shared Folder",
          2003, 20, TRUE, 0, L"My Shared Folder");
    check("05-09-2003  12:01AM 12656686 file with spaces.rgz",
          2003, 0, FALSE, 12656686, L"file with spaces.rgz");
    check("05-09-99 12:45PM 4294967295 a.txt",
          1999, 12, FALSE, 0xffffffffu, L"a.txt");
    check("12-01-08 09:00AM 4096 Textfile.txt",
          2008, 9, FALSE, 4096, L"Textfile.txt");
    check("01-08-09 08:01PM <DIR> SomeFolder",
          2009, 20, TRUE, 0, L"SomeFolder");
    for (i = 0; bad[i]; i++)
        if (FTP_ParseDOSListing(bad[i], &props))
        {
            fprintf(stderr, "accepted invalid IIS listing: %s\n", bad[i]);
            abort();
        }
    puts("IIS FTP LIST parser passed");
    return 0;
}
"""


class FtpDirectoryListingTests(unittest.TestCase):
    def test_iis_first_then_unix(self):
        source = SOURCE.read_text()
        self.assertIn("if (!FTP_ParseDOSListing(pszLine, lpfp))", source)
        self.assertNotIn("8 == strlen(pszToken)", source)

    def test_actual_parser(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("C compiler unavailable")
        text = HARNESS.replace("__FUNCTION__", extract(SOURCE.read_text()))
        with tempfile.TemporaryDirectory(prefix="water-ftp-iis-") as work:
            src, exe = Path(work) / "iis_ftp.c", Path(work) / "iis_ftp"
            src.write_text(text)
            result = subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                                                str(src), "-o", str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("IIS FTP LIST parser passed", run.stdout)


if __name__ == "__main__":
    unittest.main()
