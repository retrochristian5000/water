#!/usr/bin/env python3
"""Bounded MZ/LE inspector regression tests, using artificial fixtures only."""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

SOURCE = Path(__file__).resolve().parents[1] / "inspect-vxd.py"
SPEC = importlib.util.spec_from_file_location("inspect_vxd", SOURCE)
mod = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = mod
SPEC.loader.exec_module(mod)


def sample():
    b = bytearray(0x240)
    b[:2] = b"MZ"
    struct.pack_into("<I", b, 0x3c, 0x80)
    off = 0x80
    b[off:off + 2] = b"LE"
    struct.pack_into("<H", b, off + 0x08, 2)  # 80386
    struct.pack_into("<I", b, off + 0x40, 0xac)
    struct.pack_into("<I", b, off + 0x44, 1)
    struct.pack_into("<I", b, off + 0x58, 0xc4)
    struct.pack_into("<I", b, off + 0x5c, 0xe0)
    pos = off + 0xc4
    b[pos] = 7
    b[pos + 1:pos + 8] = b"CWBMIDI"
    struct.pack_into("<H", b, pos + 8, 0)
    b[pos + 10] = 0
    return b


class InspectorTests(unittest.TestCase):
    def test_valid_and_stable(self):
        result = mod.inspect(bytes(sample()))
        self.assertEqual(result["format"], "MZ/LE")
        self.assertEqual(result["cpu_type"], 2)
        self.assertEqual(result["object_count"], 1)
        self.assertEqual(result["resident_names"],
                         [{"name": "CWBMIDI", "ordinal": 0}])
        self.assertIsNone(result["vxd_device_id"])
        self.assertEqual(len(result["sha256"]), 64)

    def test_wrong_magic(self):
        b = sample()
        b[0] = 0
        with self.assertRaisesRegex(mod.VxdError, "MZ"):
            mod.inspect(b)
        b = sample()
        b[0x80:0x82] = b"PE"
        with self.assertRaisesRegex(mod.VxdError, "LE image"):
            mod.inspect(b)

    def test_truncated_and_out_of_bounds(self):
        with self.assertRaises(mod.VxdError):
            mod.inspect(b"MZ")
        b = sample()
        struct.pack_into("<I", b, 0x3c, 0xffffff00)
        with self.assertRaises(mod.VxdError):
            mod.inspect(b)
        b = sample()
        struct.pack_into("<I", b, 0x80 + 0x44, 4097)
        with self.assertRaisesRegex(mod.VxdError, "object"):
            mod.inspect(b)

    def test_bad_resident_table(self):
        b = sample()
        b[0x80 + 0xc4] = 30  # Crosses next table boundary.
        with self.assertRaisesRegex(mod.VxdError, "truncated"):
            mod.inspect(b)
        b = sample()
        struct.pack_into("<I", b, 0x80 + 0x58, 1)
        with self.assertRaisesRegex(mod.VxdError, "resident"):
            mod.inspect(b)

    def test_missing_resident_names(self):
        b = sample()
        struct.pack_into("<I", b, 0x80 + 0x58, 0)
        self.assertEqual(mod.inspect(b)["resident_names"], [])


if __name__ == "__main__":
    unittest.main()
