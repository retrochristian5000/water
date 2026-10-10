#!/usr/bin/env python3
"""Guard MSVCRT's architecture-specific stat export aliases.

Run: python3 scripts/tests/test_msvcrt_stat_exports.py

This checks source and .spec consistency without requiring a Windows guest.
The Win64 runtime behavior is also tested by dlls/msvcrt/tests/file.c.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = ROOT / "dlls/msvcrt/msvcrt.spec"
SOURCE = ROOT / "dlls/msvcrt/file.c"

ALIASES = {
    "_fstat": ("long ptr", "_fstat32", "_fstat64i32"),
    "_fstati64": ("long ptr", "_fstat32i64", "_fstat64"),
    "_stat": ("str ptr", "_stat32", "_stat64i32"),
    "_stati64": ("str ptr", "_stat32i64", "_stat64"),
    "_wstat": ("wstr ptr", "_wstat32", "_wstat64i32"),
    "_wstati64": ("wstr ptr", "_wstat32i64", "_wstat64"),
}


class MsvcrtStatAbiTests(unittest.TestCase):
    def test_win32_and_win64_aliases(self):
        lines = [s.strip() for s in SPEC.read_text(encoding="utf-8").splitlines()]
        for public_name, (arguments, impl32, impl64) in ALIASES.items():
            with self.subTest(public_name=public_name):
                for arch, implementation in (("win32", impl32), ("win64", impl64)):
                    expected = f"@ cdecl -arch={arch} {public_name}({arguments}) {implementation}"
                    self.assertEqual(lines.count(expected), 1, expected)
                matches = [
                    s for s in lines
                    if re.match(r"^@\s+cdecl\b.*\s" + re.escape(public_name) + r"\(", s)
                ]
                self.assertEqual(len(matches), 2, (public_name, matches))

    def test_implementations_exist(self):
        source = SOURCE.read_text(encoding="utf-8")
        for _, impl32, impl64 in ALIASES.values():
            for function in (impl32, impl64):
                with self.subTest(implementation=function):
                    pattern = r"int\s+CDECL\s+" + re.escape(function) + r"\s*\("
                    self.assertRegex(source, pattern)

    def test_other_arm64_boundaries_untouched(self):
        contents = SPEC.read_text(encoding="utf-8")
        self.assertIn("@ stdcall -arch=!i386 __C_specific_handler(", contents)
        self.assertIn("@ cdecl _get_environ(ptr)", contents)
        self.assertIn("@ cdecl _get_wenviron(ptr)", contents)
        self.assertNotIn("@ cdecl __chkstk_arm64ec(", contents)


if __name__ == "__main__":
    unittest.main()
