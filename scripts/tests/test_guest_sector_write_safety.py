#!/usr/bin/env python3
"""Reject host-partition sector writes through DOS and NTVDM bridges."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t BYTE;
typedef uint32_t DWORD;
typedef int BOOL;
#define FALSE 0
#define TRUE 1

__DEM__
__KRNL__

int main(void)
{
    const BYTE data[512] = {0x5a};
    unsigned drive;
    for (drive = 0; drive < 26; drive++)
    {
        assert(!DEM_AbsoluteWrite(drive, 0, 1, data, FALSE));
        assert(!DEM_AbsoluteWrite(drive, 0, 1, data, TRUE));
        assert(!DOSVM_RawWrite(drive, 0, 1, data, FALSE));
        assert(!DOSVM_RawWrite(drive, 0, 1, data, TRUE));
    }
    assert(!DEM_AbsoluteWrite(2, 0xffffffffu, 0xffffffffu, data, TRUE));
    assert(!DOSVM_RawWrite(2, 0xffffffffu, 0xffffffffu, data, TRUE));
    assert(data[0] == 0x5a);
    puts("DOS sector writes denied without guest image");
    return 0;
}
"""


def extract(source, name):
    match = re.search(r"^BOOL\s+" + re.escape(name) + r"\s*\([^;]*\)\s*\{",
                      source, re.MULTILINE)
    if not match:
        raise AssertionError("Missing function " + name)
    depth = 0
    for i in range(match.end() - 1, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():i + 1]
    raise AssertionError("Unterminated function " + name)


class GuestSectorSafetyTests(unittest.TestCase):
    def test_source_boundary(self):
        krnl = (ROOT / "dlls/krnl386.exe16/ntdos_absdisk_compat.c").read_text()
        dem = (ROOT / "programs/ntvdm/dem_disk.c").read_text()
        absdisk = (ROOT / "programs/ntdos.sys/absdisk.c").read_text()
        int13 = (ROOT / "programs/ntio.sys/int13.c").read_text()
        self.assertNotIn("krnl386_legacy_absolute_write(", krnl)
        for src in (krnl, dem):
            self.assertNotIn("GENERIC_WRITE", src)
            self.assertNotIn("WriteFile(", src)
        self.assertIn("DEM_AbsoluteWrite(AL_reg(context), begin, length, dataptr, FALSE)", absdisk)
        self.assertIn("DOSVM_RawWrite(drive.dos_drive, lba, count, buffer, FALSE)", int13)
        self.assertIn("BOOL DEM_AbsoluteRead(", dem)
        self.assertIn("BOOL DOSVM_RawRead(", krnl)
        self.assertIn("return FALSE;", extract(dem, "DEM_AbsoluteWrite"))
        self.assertIn("return FALSE;", extract(krnl, "DOSVM_RawWrite"))

    def test_compiled_handlers_deny_all_drives(self):
        cc = shlex.split(os.environ.get("CC", "cc"))
        if not cc or not shutil.which(cc[0]):
            self.skipTest("No C compiler")
        krnl = (ROOT / "dlls/krnl386.exe16/ntdos_absdisk_compat.c").read_text()
        dem = (ROOT / "programs/ntvdm/dem_disk.c").read_text()
        src = HARNESS.replace("__DEM__", extract(dem, "DEM_AbsoluteWrite"))
        src = src.replace("__KRNL__", extract(krnl, "DOSVM_RawWrite"))
        with tempfile.TemporaryDirectory(prefix="water-guest-disk-") as tmp:
            inp = Path(tmp) / "test.c"
            exe = Path(tmp) / "test"
            inp.write_text(src)
            built = subprocess.run(cc + ["-std=c99", "-Wall", "-Wextra",
                                          "-Werror", str(inp), "-o", str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            done = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(done.returncode, 0, done.stderr)
            self.assertIn("DOS sector writes denied", done.stdout)


if __name__ == "__main__":
    unittest.main()
