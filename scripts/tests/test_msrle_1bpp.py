#!/usr/bin/env python3
"""Compile real Water 1bpp MSRLE encoder expressions against MSB-first test cases."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "dlls/msrle32/msrle32.c"

C_HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef int32_t LONG;
struct fake_info { LONG biWidth; WORD biBitCount; };
__MACRO__

static void convert_1bpp(LONG width, const BYTE *lpIn, WORD *lpOut)
{
    struct fake_info info = {width, 1};
    struct fake_info *lpbiIn = &info;
    WORD wIntensityTbl[2] = {0x1234, 0x5678};
    LONG x;
    __INNER_LOOP__
}

int main(void)
{
    const BYTE raw[] = {0x81, 0x40};
    struct fake_info info = {10, 1};
    WORD got[12] = {0};
    int x;
    const int expected[] = {1,0,0,0,0,0,0,1,0,1};
    for (x = 0; x < 10; x++)
        assert(GetRawPixel(&info, raw, x) == expected[x]);
    convert_1bpp(10, raw, got);
    for (x = 0; x < 10; x++)
        assert(got[x] == (expected[x] ? 0x5678 : 0x1234));
    assert(got[10] == 0 && got[11] == 0); /* no stray writes */
    puts("MSRLE 1bpp MSB-first conversion passed");
    return 0;
}
"""

def real_snippets(source):
    a = source.index("#define GetRawPixel(lpbi,lp,x)")
    b = source.index("\n\n", a)
    macro = source[a:b]
    start = source.index("void computeInternalFrame(")
    case = source.index("    case 1:", start)
    end = source.index("      break;", case)
    loop = source[case + len("    case 1:"):end].strip()
    return macro, loop


class TestMsrc1bpp(unittest.TestCase):
    def test_source_unchanged_other_bpp(self):
        s = SOURCE.read_text()
        macro, loop = real_snippets(s)
        self.assertIn("7 - (x)%8", macro)
        self.assertIn("x < lpbiIn->biWidth", loop)
        self.assertIn("lpIn[x / 8]", loop)
        self.assertIn("case 4:", s)
        self.assertIn("case 8:", s)

    def test_compiled_encoder_expression(self):
        cmd = shlex.split(os.environ.get("CC", "cc"))
        if not cmd or not shutil.which(cmd[0]):
            self.skipTest("C compiler not available")
        macro, loop = real_snippets(SOURCE.read_text())
        code = C_HARNESS.replace("__MACRO__", macro).replace("__INNER_LOOP__", loop)
        with tempfile.TemporaryDirectory(prefix="water-rle-1bpp-") as d:
            c_file = Path(d) / "test.c"
            binary = Path(d) / "test"
            c_file.write_text(code)
            built = subprocess.run(cmd + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                                          str(c_file), "-o", str(binary)],
                                   capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            run = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("MSRLE 1bpp MSB-first conversion passed", run.stdout)

if __name__ == "__main__":
    unittest.main()
