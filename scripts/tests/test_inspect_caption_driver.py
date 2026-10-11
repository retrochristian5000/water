#!/usr/bin/env python3
"""Synthetic Win9x driver inventory tests; not an actual WDM decoder test."""
import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "inspect-caption-driver.py"
SPEC = importlib.util.spec_from_file_location("inspect_caption_driver", SOURCE)
cap = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(cap)


class CaptionDriverInventoryTests(unittest.TestCase):
    def test_inf_sections_and_sha(self):
        with tempfile.TemporaryDirectory() as td:
            folder = Path(td)
            (folder / "capture.inf").write_bytes(
                b"[Version]\r\nSignature=\"$CHICAGO$\"\r\n"
                b"[Driver]\r\nCopyFiles=ccdecode.sys ; device codec\r\n"
                b"; Closed Caption Decoder comment should not match\r\n")
            output = cap.inventory([folder])
            self.assertEqual(output["files_examined"], 1)
            self.assertEqual(len(output["candidates"]), 1)
            record = output["candidates"][0]
            self.assertEqual(record["kind"], "candidate-inf")
            self.assertEqual(record["matches"][0]["section"], "Driver")
            self.assertEqual(record["matches"][0]["line"], 4)
            self.assertEqual(len(record["matches"]), 1)
            self.assertEqual(len(record["sha256"]), 64)

    def test_ccdecode_sys_pe32_x86_and_pe32plus_arm64(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "CCDECODE.SYS"
            image = bytearray(512)
            image[:2] = b"MZ"
            struct.pack_into("<I", image, 0x3c, 0x80)
            image[0x80:0x84] = b"PE\x00\x00"
            struct.pack_into("<H", image, 0x84, 0x14c)
            struct.pack_into("<H", image, 0x98, 0x10b)
            path.write_bytes(image)
            d = cap.scan_file(path)
            self.assertEqual((d["format"], d["machine"]), ("PE32", "0x014c"))
            struct.pack_into("<H", image, 0x84, 0xaa64)
            struct.pack_into("<H", image, 0x98, 0x20b)
            path.write_bytes(image)
            d = cap.scan_file(path)
            self.assertEqual((d["format"], d["machine"]), ("PE32+", "0xaa64"))

    def test_no_false_positives_or_fake_pe(self):
        with tempfile.TemporaryDirectory() as td:
            folder = Path(td)
            (folder / "other.inf").write_text("; CCDECODE.SYS\n[Version]\nProvider=Microsoft\n")
            (folder / "CCDECODE.SYS").write_bytes(b"not a driver")
            output = cap.inventory([folder])
            self.assertEqual(len(output["candidates"]), 1)
            self.assertEqual(output["candidates"][0]["format"], "not-MZ")

    def test_ccdecode_inf_without_matching_text(self):
        with tempfile.TemporaryDirectory() as td:
            path = Path(td) / "CCDECODE.INF"
            path.write_bytes(b"[Version]\r\nSignature=\"$CHICAGO$\"\r\n")
            self.assertEqual(cap.scan_file(path)["matches"], [])

    def test_missing_path(self):
        with self.assertRaisesRegex(ValueError, "missing input"):
            cap.inventory([Path("/definitely/unavailable/whp-caption-file")])


if __name__ == "__main__":
    unittest.main()
