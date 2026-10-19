#!/usr/bin/env python3
"""Check Win95 INT13.VXD compatibility boundary without touching host disks."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]

class Int13BoundaryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.src = (ROOT / "dlls/int13.vxd/int13.c").read_text()
        cls.spec = (ROOT / "dlls/int13.vxd/int13.vxd.spec").read_text()
        cls.make = (ROOT / "dlls/int13.vxd/Makefile.in").read_text()
        cls.config = (ROOT / "configure.ac").read_text()
        cls.router = (ROOT / "dlls/krnl386.exe16/vxd.c").read_text()
        cls.ntio = (ROOT / "programs/ntio.sys/int13.c").read_text()

    def test_build_and_device_id(self):
        self.assertIn("WINE_CONFIG_MAKEFILE(dlls/int13.vxd)", self.config)
        self.assertIn("MODULE    = int13.vxd", self.make)
        self.assertIn("INT13_DeviceIoControl", self.spec)
        self.assertIn("INT13_VxDCall", self.spec)
        self.assertRegex(self.router,
                         r"\{'i','n','t','1','3','\.','v','x','d',0\},\s*0x0020")
        self.assertRegex(self.router,
                         r"\{'i','o','s','\.','v','x','d',0\},\s*0x0010")

    def test_fail_closed_and_no_host_io(self):
        self.assertIn("SetLastError(ERROR_INVALID_FUNCTION)", self.src)
        self.assertIn("if (returned) *returned = 0;", self.src)
        self.assertIn("return FALSE;", self.src)
        self.assertIn("return 0xffffffff;", self.src)
        for call in ("DOSVM_RawRead(", "DOSVM_RawWrite(", "CreateFileW(",
                     "WriteFile(", "ReadFile(", "DOSVM_Int13Handler("):
            self.assertNotIn(call, self.src)

    def test_bios_emulator_is_separate(self):
        self.assertIn("void WINAPI DOSVM_Int13Handler(", self.ntio)
        self.assertIn("DOSVM_RawRead(", self.ntio)
        self.assertIn("DOSVM_RawWrite(", self.ntio)
        self.assertNotIn('#include "../../programs/ntio.sys/int13.c"', self.src)

if __name__ == "__main__":
    unittest.main()
