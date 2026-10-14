#!/usr/bin/env python3
"""Exercise ARM64EC COFF-symbol auditing with mocked symbol tables."""
import importlib.util
from pathlib import Path
import unittest

script = Path(__file__).resolve().parents[1] / "check-libcxx-symbols.py"
spec = importlib.util.spec_from_file_location("check_libcxx_symbols", script)
audit_module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit_module)


class SymbolAuditTests(unittest.TestCase):
    def setUp(self):
        self.x64 = "?lock@mutex@__1@std@@QEAAXXZ"
        self.ec = "?lock@mutex@__1@std@@$$hQEAAXXZ"

    def test_native_x64_cannot_satisfy_arm64ec_symbol(self):
        result = audit_module.audit({self.x64: {"T"}}, {}, {self.ec: {"U"}},
                                    "arm64ec-pc-windows-msvc")
        self.assertEqual(result["missing_required_libcxx_symbols"], [self.ec])

    def test_correct_arm64ec_provider(self):
        result = audit_module.audit({self.ec: {"T"}}, {},
                                    {self.ec: {"U"}, "__CxxFrameHandler4": {"U"}},
                                    "arm64ec-pc-windows-msvc")
        self.assertFalse(result["missing_required_libcxx_symbols"])
        self.assertEqual(result["other_provider_references_diagnostic"],
                         ["__CxxFrameHandler4"])

    def test_weak_alias_is_not_strong_export_requirement(self):
        result = audit_module.audit({self.x64: {"T"}}, {},
                                    {self.ec: {"w"}, self.x64: {"U"}},
                                    "arm64ec-pc-windows-msvc")
        self.assertFalse(result["missing_required_libcxx_symbols"])
        self.assertEqual(result["weak_libcxx_references_diagnostic"], [self.ec])

    def test_archive_unused_member_is_only_diagnostic(self):
        other = "?extra@__1@std@@YAXXZ"
        result = audit_module.audit({self.x64: {"T"}}, {other: {"U"}},
                                    {self.x64: {"U"}}, "x86_64-pc-windows-msvc")
        self.assertFalse(result["missing_required_libcxx_symbols"])
        self.assertEqual(result["archive_unresolved_libcxx_references_diagnostic"],
                         [other])

    def test_llvm_nm_posix_archive_heading(self):
        result = audit_module.parse_posix(
            "lib.a[mutex.obj]:\n" + self.ec + " T 0 20\n" + self.x64 + " U\n")
        self.assertEqual(result, {self.ec: {"T"}, self.x64: {"U"}})


if __name__ == "__main__":
    unittest.main()
