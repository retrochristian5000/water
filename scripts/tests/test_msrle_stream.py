#!/usr/bin/env python3
"""Compile the actual Water RLE4/RLE8 packet validator with synthetic streams."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "dlls/msrle32/msrle32.c"
NAME = "MSRLE32_ValidateCompressedStream"

def extract(source):
    match = re.search(r"static BOOL\s+" + NAME + r"\s*\([^;]*\)\s*\{", source)
    if not match:
        raise AssertionError("Validator definition missing")
    depth = 0
    for index in range(match.end() - 1, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if not depth:
                return source[match.start():index + 1]
    raise AssertionError("Unterminated validator")

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
__VALIDATOR__

static void test(const BYTE *in, DWORD len, LONG width, LONG height, WORD bpp, int expect)
{
    BYTE copy[128];
    assert(len <= sizeof(copy));
    if (in) memcpy(copy, in, len);
    assert(MSRLE32_ValidateCompressedStream(in ? copy : NULL, len, width, height, bpp) == expect);
    if (in) assert(!memcmp(copy, in, len));
}
int main(void)
{
    const BYTE run8[]       = {3,9,0,0,3,7,0,1};
    const BYTE run4[]       = {5,0x12,0,1};
    const BYTE literal8[]   = {0,3,1,2,3,0,0,1};
    const BYTE literal4[]   = {0,5,0x12,0x34,0x50,0,0,1};
    const BYTE delta[]      = {0,2,2,1,2,9,0,1};
    const BYTE eol_eoi[]    = {1,2,0,0,0,1};
    const BYTE overrun[]    = {6,2,0,1};
    const BYTE badlit[]     = {0,5,1,2,3,4,5,0,0,1};
    const BYTE bad_delta[]  = {0,2,1,2,0,1};
    const BYTE wrap[]       = {0,2,3,0,1,5,0,1};
    const BYTE noend[]      = {1,5,0,0};
    const BYTE shortpair[]  = {0};
    const BYTE shortdelta[] = {0,2,1};
    const BYTE shortlit[]   = {0,3,1,2,3};
    const BYTE badpad[]     = {0,5,0x12,0x34,0x50,0,1};
    const BYTE late[]       = {1,3,0,0,1,4,0,1};
    test(run8,sizeof(run8),3,2,8,TRUE);
    test(run4,sizeof(run4),5,1,4,TRUE);
    test(literal8,sizeof(literal8),3,1,8,TRUE);
    test(literal4,sizeof(literal4),5,1,4,TRUE);
    test(delta,sizeof(delta),6,3,8,TRUE);
    test(eol_eoi,sizeof(eol_eoi),1,1,8,TRUE);
    test(overrun,sizeof(overrun),5,1,8,FALSE);
    test(badlit,sizeof(badlit),4,1,8,FALSE);
    test(bad_delta,sizeof(bad_delta),3,2,8,FALSE);
    test(wrap,sizeof(wrap),3,1,8,FALSE);
    test(noend,sizeof(noend),1,1,8,FALSE);
    test(shortpair,sizeof(shortpair),1,1,8,FALSE);
    test(shortdelta,sizeof(shortdelta),2,2,8,FALSE);
    test(shortlit,sizeof(shortlit),3,1,8,FALSE);
    test(badpad,sizeof(badpad),5,1,4,FALSE);
    test(late,sizeof(late),1,1,8,FALSE);
    test(run4,sizeof(run4),5,-1,4,FALSE);
    test(run4,sizeof(run4),5,1,8,FALSE);
    test(NULL,0,5,1,4,FALSE);
    puts("MSRLE preflight synthetic cases passed");
    return 0;
}
"""

class TestRle(unittest.TestCase):
    def test_wiring(self):
        text = SOURCE.read_text()
        body = extract(text)
        self.assertIn("padded > size - pos", body)
        self.assertIn("return FALSE; /* missing explicit end of image */", body)
        call = text.index("!MSRLE32_ValidateCompressedStream(pic->lpInput")
        self.assertLess(call, text.index("/* check formats */", call))
        self.assertLess(call, text.index("pic->lpbiOutput->biSizeImage =", call))

    def test_compiled_actual_preflight(self):
        cmd = shlex.split(os.environ.get("CC", "cc"))
        if not cmd or not shutil.which(cmd[0]):
            self.skipTest("No C compiler available")
        code = HARNESS.replace("__VALIDATOR__", extract(SOURCE.read_text()))
        with tempfile.TemporaryDirectory(prefix="msrle-preflight-") as directory:
            c_file = Path(directory) / "test.c"
            exe = Path(directory) / "test"
            c_file.write_text(code)
            result = subprocess.run(cmd + ["-std=c99", "-Wall", "-Wextra",
                                            "-Werror", str(c_file), "-o", str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("MSRLE preflight synthetic cases passed", result.stdout)

if __name__ == "__main__":
    unittest.main()
