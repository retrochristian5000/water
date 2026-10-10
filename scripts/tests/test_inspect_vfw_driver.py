#!/usr/bin/env python3
"""Synthetic PE image tests; no actual VDONet binary or executable is used."""
from pathlib import Path
import importlib.util
import struct
import sys
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "inspect-vfw-driver.py"
SPEC = importlib.util.spec_from_file_location("inspect_vfw_driver", SOURCE)
mod = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = mod
SPEC.loader.exec_module(mod)


def w16(b, at, value):
    struct.pack_into("<H", b, at, value)

def w32(b, at, value):
    struct.pack_into("<I", b, at, value)


def sample():
    b = bytearray(0x400)
    b[:2] = b"MZ"
    w32(b, 0x3c, 0x80)
    b[0x80:0x84] = b"PE\0\0"
    w16(b, 0x84, 0x14c)
    w16(b, 0x86, 1)
    w16(b, 0x94, 0xe0)
    w16(b, 0x96, 0x210e)  # IMAGE_FILE_DLL
    w16(b, 0x98, 0x10b)
    w32(b, 0x98 + 92, 16)
    w32(b, 0x98 + 96, 0x1000)
    w32(b, 0x98 + 100, 0x90)
    # One .edata section at virtual address 0x1000, raw offset 0x200.
    base = 0x98 + 0xe0
    b[base:base + 6] = b".edata"
    w32(b, base + 8, 0x200)
    w32(b, base + 12, 0x1000)
    w32(b, base + 16, 0x200)
    w32(b, base + 20, 0x200)
    e = 0x200
    w32(b, e + 12, 0x1050)  # DLL name
    w32(b, e + 16, 1)  # export ordinal base
    w32(b, e + 20, 1)  # function count
    w32(b, e + 24, 1)  # name count
    w32(b, e + 28, 0x1060)
    w32(b, e + 32, 0x1064)
    w32(b, e + 36, 0x1068)
    b[0x250:0x25c] = b"vdowave.drv\0"
    w32(b, 0x260, 0x1100)
    w32(b, 0x264, 0x1070)
    w16(b, 0x268, 0)
    b[0x270:0x27b] = b"DriverProc\0"
    return b


class InspectVfwDriverTests(unittest.TestCase):
    def test_valid_pe_driver_and_unknown_codec(self):
        d = mod.inspect(sample())
        self.assertEqual(d["format"], "PE32")
        self.assertEqual(d["machine"], "i386")
        self.assertTrue(d["dll_characteristic"])
        self.assertEqual(d["image_name"], "vdowave.drv")
        self.assertEqual(d["exports"], [{"name": "DriverProc", "ordinal": 1,
                                        "forwarder": None}])
        self.assertTrue(d["has_DriverProc"])
        self.assertIsNone(d["codec_fourcc"])
        self.assertEqual(len(d["sha256"]), 64)

    def test_valid_with_no_exports(self):
        b = sample()
        w32(b, 0x98 + 96, 0)
        d = mod.inspect(b)
        self.assertFalse(d["has_DriverProc"])
        self.assertEqual(d["exports"], [])

    def test_bad_formats(self):
        for change in (lambda b: b.__setitem__(0, 0),
                       lambda b: w32(b, 0x3c, 0xfffffff0),
                       lambda b: b.__setitem__(slice(0x80, 0x84), b"NE\0\0"),
                       lambda b: b.__setitem__(slice(0x80, 0x84), b"LX\0\0"),
                       lambda b: w16(b, 0x96, 0)):
            b = sample()
            change(b)
            if b[0x96:0x98] == b"\0\0":
                self.assertFalse(mod.inspect(b)["dll_characteristic"])
            else:
                with self.assertRaises(mod.DriverFormatError):
                    mod.inspect(b)

    def test_bad_section_and_export_tables(self):
        for change in (lambda b: w16(b, 0x86, 97),
                       lambda b: w16(b, 0x94, 0xffff),
                       lambda b: w32(b, 0x200 + 24, 3),
                       lambda b: w16(b, 0x268, 1),
                       lambda b: w32(b, 0x264, 0xffffff00),
                       lambda b: b.__setitem__(slice(0x270, 0x400), b"X" * (0x400 - 0x270))):
            b = sample()
            change(b)
            with self.assertRaises(mod.DriverFormatError):
                mod.inspect(b)

    def test_missing_dll_flag_not_auto_accepted(self):
        b = sample()
        w16(b, 0x96, 0x010e)
        self.assertFalse(mod.inspect(b)["dll_characteristic"])


if __name__ == "__main__":
    unittest.main()
