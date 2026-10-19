#!/usr/bin/env python3
"""Compile Water's real SETUPX ordinal 312 selector with synthetic device data."""
from pathlib import Path
import os
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
CODE = ROOT / "dlls/setupx.dll16/devinst16.c"
HDR = ROOT / "dlls/setupx.dll16/setupx16.h"
SPEC = ROOT / "dlls/setupx.dll16/setupx.dll16.spec"

def func(s):
    i = s.index("RETERR16 WINAPI DiSelectDevice16(")
    j = s.index("{", i)
    depth = 0
    for k in range(j, len(s)):
        if s[k] == "{": depth += 1
        if s[k] == "}":
            depth -= 1
            if depth == 0: return s[i:k + 1]
    raise AssertionError("unterminated selector")

HARNESS = r"""
#include <stdio.h>
#include <assert.h>
#include <string.h>
typedef unsigned short RETERR16;
typedef unsigned short UINT16;
#define WINAPI
#define OK 0
#define DI_ERROR 500
__ENUM__
typedef struct device_info_mock {
    UINT16 cbSize;
    void *lpCompatDrvList;
    void *lpClassDrvList;
    void *lpSelectedDriver;
} DEVICE_INFO16, *LPDEVICE_INFO16;
__FUNC__
static void fail_without_change(DEVICE_INFO16 *d, RETERR16 error)
{
    DEVICE_INFO16 old = *d;
    assert(DiSelectDevice16(d) == error);
    assert(!memcmp(d, &old, sizeof(old)));
}
int main(void)
{
    int comp = 1, klass = 2, foreign = 3;
    DEVICE_INFO16 d = {0};
    assert(DiSelectDevice16(0) == ERR_DI_BAD_DEV_INFO);
    d.cbSize = sizeof(d) - 1;
    fail_without_change(&d, ERR_DI_BAD_DEV_INFO);
    d.cbSize = sizeof(d) + 1;
    fail_without_change(&d, ERR_DI_BAD_DEV_INFO);
    d.cbSize = sizeof(d);
    fail_without_change(&d, ERR_DI_BAD_SELECTDEVICE_PARAMS);
    d.lpCompatDrvList = &comp;
    fail_without_change(&d, ERR_DI_BAD_SELECTDEVICE_PARAMS);
    d.lpSelectedDriver = &foreign;
    fail_without_change(&d, ERR_DI_BAD_SELECTDEVICE_PARAMS);
    d.lpSelectedDriver = &comp;
    assert(DiSelectDevice16(&d) == OK && d.lpSelectedDriver == &comp);
    d.lpClassDrvList = &klass;
    d.lpSelectedDriver = &klass;
    assert(DiSelectDevice16(&d) == OK && d.lpSelectedDriver == &klass);
    d.lpCompatDrvList = 0;
    d.lpClassDrvList = 0;
    fail_without_change(&d, ERR_DI_BAD_SELECTDEVICE_PARAMS);
    d.lpSelectedDriver = 0;
    fail_without_change(&d, ERR_DI_BAD_SELECTDEVICE_PARAMS);
    puts("setupx312 synthetic selection passed");
    return 0;
}
"""

class SelectionTests(unittest.TestCase):
    def test_export_and_safety(self):
        self.assertIn("312  pascal -ret16 DiSelectDevice(ptr) DiSelectDevice16",
                      SPEC.read_text())
        self.assertIn("extern RETERR16 WINAPI DiSelectDevice16(LPDEVICE_INFO16);",
                      HDR.read_text())
        s = func(CODE.read_text())
        self.assertIn("ERR_DI_BAD_DEV_INFO", s)
        self.assertIn("ERR_DI_BAD_SELECTDEVICE_PARAMS", s)
        self.assertIn("lpSelectedDriver == lpdi->lpCompatDrvList", s)
        self.assertIn("lpSelectedDriver == lpdi->lpClassDrvList", s)
        self.assertNotRegex(s, r"lpdi->lpSelectedDriver\s*=(?!=)")
        for forbidden in ("RegSetValue", "CreateFile", "DiInstallDevice(",
                          "SetupDiSelectDevice(", "WriteFile"):
            self.assertNotIn(forbidden, s)

    def test_compiled_synthetic_paths(self):
        cmd = shlex.split(os.environ.get("CC", "cc"))
        if not cmd or not shutil.which(cmd[0]):
            self.skipTest("no host C compiler")
        m = re.search(r"enum _ERR_DEVICE_INSTALL\s*\{[^}]+\};", HDR.read_text())
        self.assertIsNotNone(m)
        harness = HARNESS.replace("__ENUM__", m.group(0))
        harness = harness.replace("__FUNC__", func(CODE.read_text()))
        with tempfile.TemporaryDirectory(prefix="water-setupx312-") as dirname:
            cfile = Path(dirname) / "test.c"
            exe = Path(dirname) / "test"
            cfile.write_text(harness)
            result = subprocess.run(cmd + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                                           str(cfile), "-o", str(exe)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("setupx312 synthetic selection passed", result.stdout)

if __name__ == "__main__":
    unittest.main()
