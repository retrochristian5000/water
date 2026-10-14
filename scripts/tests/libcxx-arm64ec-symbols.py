#!/usr/bin/env python3
"""Exercise ARM64EC COFF-symbol auditing with mocked symbol tables."""
import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
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

    def test_weak_arm64ec_entry_requires_target_provider(self):
        result = audit_module.audit({self.x64: {"T"}}, {},
                                    {self.ec: {"w"}, self.x64: {"w"}},
                                    "arm64ec-pc-windows-msvc")
        self.assertEqual(result["missing_arm64ec_weak_entry_symbols"], [self.ec])
        self.assertFalse(result["missing_required_libcxx_symbols"])

    def test_weak_arm64ec_entry_is_present(self):
        result = audit_module.audit({self.ec: {"T"}}, {},
                                    {self.ec: {"w"}, self.x64: {"w"}},
                                    "arm64ec-pc-windows-msvc")
        self.assertFalse(result["missing_arm64ec_weak_entry_symbols"])

    def test_locally_defined_arm64ec_weak_symbol_is_not_required(self):
        result = audit_module.audit({self.x64: {"T"}}, {},
                                    {self.ec: {"w"}},
                                    "arm64ec-pc-windows-msvc",
                                    {self.ec: {"T"}})
        self.assertFalse(result["missing_arm64ec_weak_entry_symbols"])

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


    def test_build_checks_cached_and_new_arm64ec_archives(self):
        build = (Path(__file__).resolve().parents[2] / "build.sh").read_text(
            encoding="utf-8")
        self.assertEqual(build.count(
            'audit_llvm_libcxx_arm64ec_symbols "$whp_libcxx_provider/libwhp-libcxx.a"'), 2)

    @unittest.skipUnless(shutil.which("clang++") and shutil.which("llvm-objdump"),
                         "Clang and llvm-objdump required for COFF probe")
    def test_real_arm64ec_clang_emits_hybrid_mangling(self):
        # Standalone, without libc++: confirm ABI spellings directly from COFF.
        with tempfile.TemporaryDirectory(prefix="water-arm64ec-symbol-") as tmp:
            source = Path(tmp) / "probe.cpp"
            obj = Path(tmp) / "probe.obj"
            source.write_text(
                "namespace std { inline namespace __1 {"
                "struct mutex { void lock(); }; } }\\n"
                "void use(std::__1::mutex &m) { m.lock(); }\\n",
                encoding="utf-8")
            result = subprocess.run(
                [shutil.which("clang++"), "--target=arm64ec-pc-windows-msvc",
                 "-fms-extensions", "-c", str(source), "-o", str(obj)],
                capture_output=True, text=True)
            if result.returncode:
                self.skipTest("installed Clang cannot emit ARM64EC COFF")
            output = subprocess.check_output(
                [shutil.which("llvm-objdump"), "--syms", str(obj)], text=True)
            self.assertIn(self.ec, output)


if __name__ == "__main__":
    unittest.main()
