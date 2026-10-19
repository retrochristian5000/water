#!/usr/bin/env python3
"""Compile Water's IO.SYS DoubleBuffer parser; verify boot-state wiring."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

def c_function(source, name, rettype):
    regex = r"(?m)^" + re.escape(rettype) + r"\s+" + re.escape(name) + r"\s*\([^;]*\)\s*\{"
    match = re.search(regex, source)
    if not match:
        raise AssertionError("Missing function: " + name)
    depth = 0
    for index in range(match.end() - 1, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if not depth:
                return source[match.start():index + 1]
    raise AssertionError("Unterminated function: " + name)

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char BYTE;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
__PARSER__

static void invalid(const char *s)
{
    BYTE mode = 2;
    assert(!IOSYS_ParseDoubleBuffer(s, &mode));
    assert(mode == 2);
}

int main(void)
{
    BYTE mode = 3;
    assert(IOSYS_ParseDoubleBuffer("0", &mode) && mode == 0);
    assert(IOSYS_ParseDoubleBuffer("1", &mode) && mode == 1);
    assert(IOSYS_ParseDoubleBuffer("2", &mode) && mode == 2);
    assert(IOSYS_ParseDoubleBuffer("  1 \t", &mode) && mode == 1);
    assert(IOSYS_ParseDoubleBuffer("\t2\t", &mode) && mode == 2);
    invalid("");
    invalid(" ");
    invalid("-1");
    invalid("3");
    invalid("01");
    invalid("2x");
    invalid("1 ; comment");
    invalid("true");
    invalid("2\n");
    invalid(NULL);
    assert(!IOSYS_ParseDoubleBuffer("1", NULL));
    puts("iosys doublebuffer: passed");
    return 0;
}
"""


class DoubleBufferTests(unittest.TestCase):
    def test_source_integration(self):
        source = (ROOT / "programs/io.sys/config.c").read_text()
        header = (ROOT / "programs/io.sys/io_sys.h").read_text()
        self.assertIn("IOSYS_ParseDoubleBuffer", header)
        self.assertIn("IOSYS_GetMsdosDoubleBufferMode", header)
        init = source[source.index("void IOSYS_InitConfig(void)"):
                      source.index("BYTE IOSYS_GetBootDrive(void)")]
        self.assertIn('GetPrivateProfileStringA( "Options", "DoubleBuffer", "0"', init)
        self.assertIn("iosys_msdos_double_buffer_mode = 0;", init)
        self.assertIn("!is_winme_personality()", init)
        self.assertIn("!IOSYS_ParseDoubleBuffer( double_buffer_value,", init)
        self.assertLess(init.index("iosys_msdos_double_buffer_mode = 0;"),
                        init.index("if (is_win3_standard_personality()"))
        self.assertLess(init.index("have_file = get_msdos_sys_path( filename );"),
                        init.index('GetPrivateProfileStringA( "Options", "DoubleBuffer", "0"'))
        # Boot policy is not an NT DLL export or a host disk write.
        self.assertNotIn("LoadLibraryA", init)
        self.assertNotIn("WriteFile(", init)

    def test_real_parser_compiled(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("C compiler unavailable")
        source = (ROOT / "programs/io.sys/config.c").read_text()
        parser = c_function(source, "IOSYS_ParseDoubleBuffer", "BOOL")
        code = HARNESS.replace("__PARSER__", parser)
        with tempfile.TemporaryDirectory(prefix="water-doublebuffer-") as directory:
            c_file = Path(directory) / "test.c"
            exe = Path(directory) / "test"
            c_file.write_text(code)
            build = subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra",
                                               "-Werror", str(c_file), "-o",
                                               str(exe)], capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("iosys doublebuffer: passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
