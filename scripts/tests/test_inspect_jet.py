#!/usr/bin/env python3
"""Synthetic PE32/PE32+ import-table tests for the read-only JET scanner."""
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "inspect-jet.py"
SPEC = importlib.util.spec_from_file_location("inspect_jet", SOURCE)
jet = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(jet)


def fixture(machine=0x14c, pe64=False, name="MSJET35.DLL", delay="ESENT.DLL"):
    b = bytearray(0x600)
    b[:2] = b"MZ"
    struct.pack_into("<I", b, 0x3c, 0x80)
    b[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HH", b, 0x84, machine, 1)
    struct.pack_into("<H", b, 0x94, 0xf0 if pe64 else 0xe0)
    opt = 0x98
    struct.pack_into("<H", b, opt, 0x20b if pe64 else 0x10b)
    if pe64:
        struct.pack_into("<Q", b, opt + 24, 0x180000000)
    else:
        struct.pack_into("<I", b, opt + 28, 0x400000)
    struct.pack_into("<I", b, opt + 60, 0x200)
    dd = opt + (112 if pe64 else 96)
    struct.pack_into("<I", b, opt + (108 if pe64 else 92), 16)
    struct.pack_into("<II", b, dd + 8, 0x1000, 40)
    struct.pack_into("<II", b, dd + 13 * 8, 0x1040, 64)
    section = opt + (0xf0 if pe64 else 0xe0)
    b[section:section + 6] = b".idata"
    struct.pack_into("<IIII", b, section + 8, 0x300, 0x1000, 0x300, 0x200)
    struct.pack_into("<I", b, 0x200 + 12, 0x1100)
    struct.pack_into("<II", b, 0x240, 1, 0x1120)
    b[0x300:0x300 + len(name) + 1] = name.encode() + b"\0"
    b[0x320:0x320 + len(delay) + 1] = delay.encode() + b"\0"
    return b


class JetInspectTests(unittest.TestCase):
    def examine(self, blob, filename="TVX.EXE"):
        with tempfile.TemporaryDirectory() as tmp:
            file = Path(tmp) / filename
            file.write_bytes(blob)
            return jet.inspect(file)

    def test_pe32_jet_red_and_delayed_blue(self):
        result = self.examine(fixture())
        self.assertEqual(result["jet_red"], ["msjet35.dll"])
        self.assertEqual(result["jet_blue"], ["esent.dll"])
        self.assertEqual(result["machine"], "0x014c")
        self.assertEqual(result["format"], "PE32")
        self.assertFalse(result["jet_red_module"])

    def test_pe32plus_arm64(self):
        result = self.examine(fixture(0xaa64, True, "MSJET40.DLL", "KERNEL32.DLL"))
        self.assertEqual(result["jet_red"], ["msjet40.dll"])
        self.assertEqual(result["jet_blue"], [])
        self.assertEqual(result["machine"], "0xaa64")
        self.assertEqual(result["format"], "PE32+")

    def test_engine_filename_distinct_from_import(self):
        result = self.examine(fixture(name="KERNEL32.DLL", delay="USER32.DLL"), "MSJET35.DLL")
        self.assertTrue(result["jet_red_module"])
        self.assertEqual(result["jet_red"], [])

    def test_not_executable(self):
        with self.assertRaisesRegex(jet.PEError, "not an MZ"):
            self.examine(b"hello")

    def test_unmapped_and_unterminated(self):
        malformed = fixture()
        struct.pack_into("<I", malformed, 0x200 + 12, 0x2000)
        with self.assertRaisesRegex(jet.PEError, "unmapped"):
            self.examine(malformed)
        malformed = fixture()
        struct.pack_into("<I", malformed, 0x98 + 96 + 8 + 4, 20)
        with self.assertRaisesRegex(jet.PEError, "unterminated"):
            self.examine(malformed)


if __name__ == "__main__":
    unittest.main()
