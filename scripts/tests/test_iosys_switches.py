#!/usr/bin/env python3
"""Compile Water's real IO.SYS SWITCHES= parser with synthetic DOS configs."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"(?m)^BOOL\s+" + re.escape(name) + r"\s*\([^;]*\)\s*\{", source)
    if not match:
        raise AssertionError(f"missing {name}")
    depth = 0
    for pos in range(match.end() - 1, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if not depth:
                return source[match.start():pos + 1]
    raise AssertionError(f"unterminated {name}")


HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef int BOOL;
typedef int INT;
#define TRUE 1
#define FALSE 0
__STRUCT__
__FUNCTION__

static void verify_invalid(const char *s, struct iosys_config_sys *config)
{
    struct iosys_config_sys old = *config;
    assert(!IOSYS_ParseSwitches(s, config));
    assert(!memcmp(config, &old, sizeof(old)));
}

int main(void)
{
    struct iosys_config_sys config = {0};

    config.switches_ebios_bytes = -1;
    assert(IOSYS_ParseSwitches("/F /K/N /W", &config));
    assert(config.switches_fast_boot && config.switches_legacy_keyboard);
    assert(config.switches_disable_boot_keys && config.switches_wina20_moved);
    assert(config.switches_ebios_bytes == -1);
    assert(IOSYS_ParseSwitches(" /e:49  /f", &config));
    assert(config.switches_ebios_bytes == 64);
    assert(IOSYS_ParseSwitches("/E", &config));
    assert(config.switches_ebios_bytes == 0);
    assert(IOSYS_ParseSwitches("/E:48", &config));
    assert(config.switches_ebios_bytes == 48);
    assert(IOSYS_ParseSwitches("/E:1023", &config));
    assert(config.switches_ebios_bytes == 1024);
    assert(IOSYS_ParseSwitches("/E:1024", &config));
    assert(config.switches_ebios_bytes == 1024);
    assert(IOSYS_ParseSwitches("", &config));
    verify_invalid("/Q", &config);
    verify_invalid("/E:", &config);
    verify_invalid("/E:47", &config);
    verify_invalid("/E:1025", &config);
    verify_invalid("/E:-1", &config);
    verify_invalid("/E:xyz", &config);
    verify_invalid("/Ftail", &config);
    verify_invalid("/F, /N", &config);
    verify_invalid("-F", &config);
    verify_invalid("/N /Q", &config);
    verify_invalid("/E:9999999999999999999999999", &config);
    verify_invalid("/F/", &config);
    verify_invalid("/E:1024garbage", &config);
    verify_invalid("/N; comment", &config);
    assert(!IOSYS_ParseSwitches(NULL, &config));
    assert(!IOSYS_ParseSwitches("/F", NULL));
    puts("iosys switches: passed");
    return 0;
}
"""


class SwitchesTests(unittest.TestCase):
    def test_source_integration(self):
        source = (ROOT / "programs/io.sys/config.c").read_text()
        start = source.index("void IOSYS_ReadConfigSys")
        body = source[start:]
        self.assertIn('!_stricmp( name, "SWITCHES" )', body)
        self.assertIn("IOSYS_ParseSwitches( value, config )", body)
        self.assertIn("config->switches_ebios_bytes = -1;", body)
        self.assertLess(body.index("if (is_winme_personality())"),
                        body.index('!_stricmp( name, "SWITCHES" )'))

    def test_parser_with_synthetic_configs(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("C compiler unavailable")
        source = (ROOT / "programs/io.sys/config.c").read_text()
        header = (ROOT / "programs/io.sys/io_sys.h").read_text()
        struct = re.search(r"struct iosys_config_sys\s*\{[^}]+\};", header)
        self.assertIsNotNone(struct)
        code = HARNESS.replace("__STRUCT__", struct.group(0))
        code = code.replace("__FUNCTION__", function(source, "IOSYS_ParseSwitches"))
        with tempfile.TemporaryDirectory(prefix="water-switches-") as directory:
            input_file = Path(directory) / "test.c"
            binary = Path(directory) / "test"
            input_file.write_text(code)
            build = subprocess.run(compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                                               str(input_file), "-o", str(binary)],
                                   text=True, capture_output=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(binary)], text=True, capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("iosys switches: passed", run.stdout)


if __name__ == "__main__":
    unittest.main()
