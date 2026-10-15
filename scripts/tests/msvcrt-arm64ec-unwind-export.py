#!/usr/bin/env python3
"""Audit and compile Water's actual _local_unwind implementation for x64/ARM64EC.

An object/COFF conformance check; does not prove hybrid Windows runtime unwind.
Requires clang-cl and llvm-objdump and a Water source checkout.
"""
from pathlib import Path
import os
import re
import shutil
import struct
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
spec = (root / "dlls/msvcrt/msvcrt.spec").read_text()
if not re.search(r"(?m)^@\s+cdecl\s+-arch=win64\s+_local_unwind\(ptr ptr\)\s*$", spec):
    sys.exit("MSVCRT: expected Win64 _local_unwind export absent or changed")

cc = os.environ.get("WATER_TEST_CLANG_CL", "clang-cl")
dump = os.environ.get("WATER_TEST_LLVM_OBJDUMP", "llvm-objdump")
for executable in (cc, dump):
    if not shutil.which(executable):
        print(f"SKIP: missing {executable}", file=sys.stderr)
        sys.exit(77)

body_regex = re.compile(
    r"\bvoid\s+__cdecl\s+_local_unwind\s*\([^;{}]*\)\s*\{[^{}]*\}",
    re.MULTILINE,
)
preamble = """\
#include <stddef.h>
__declspec(dllimport) void __stdcall RtlUnwind(
    void *frame, void *target, void *record, void *return_value);
"""
sources = {
    "x64": ("dlls/msvcrt/except_x86_64.c", "x86_64-pc-windows-msvc", 0x8664),
    "arm64ec": ("dlls/msvcrt/except_arm64ec.c", "arm64ec-pc-windows-msvc", 0xA641),
}

with tempfile.TemporaryDirectory(prefix="water-msvcrt-ec-") as tmp:
    temp = Path(tmp)
    for name, (path, triple, machine) in sources.items():
        source = (root / path).read_text()
        matches = body_regex.findall(source)
        if len(matches) != 1:
            sys.exit(f"{path}: expected exactly one _local_unwind body, found {len(matches)}")
        body = matches[0]
        if not re.search(
            r"\bRtlUnwind\s*\(\s*frame\s*,\s*target\s*,\s*NULL\s*,\s*0\s*\)",
            body,
        ):
            sys.exit(f"{path}: _local_unwind no longer delegates exactly to RtlUnwind")
        fixture = temp / f"{name}.c"
        obj = temp / f"{name}.obj"
        fixture.write_text(preamble + "\n" + body + "\n")
        subprocess.run(
            [cc, f"--target={triple}", "/nologo", "/GS-", "/c",
             str(fixture), f"/Fo{obj}"],
            check=True,
        )
        data = obj.read_bytes()
        if len(data) < 20 or struct.unpack_from("<H", data)[0] != machine:
            sys.exit(f"{triple}: unexpected COFF machine type")
        symbols = subprocess.check_output([dump, "--syms", str(obj)], text=True)
        expected = "#_local_unwind" if name == "arm64ec" else "_local_unwind"
        if not re.search(r"\)\s+0x[0-9a-fA-F]+\s+" + re.escape(expected) + r"$",
                         symbols, re.MULTILINE):
            sys.exit(f"{triple}: missing function definition {expected}")
        if "__imp_RtlUnwind" not in symbols:
            sys.exit(f"{triple}: Windows RtlUnwind DLL import missing")
        if re.search(r"\b_Unwind_", symbols):
            sys.exit(f"{triple}: unexpected Itanium unwinder symbol")
        if name == "arm64ec":
            for required in ("$ientry_thunk$cdecl$", "__os_arm64x_dispatch_ret"):
                if required not in symbols:
                    sys.exit(f"{triple}: missing ARM64EC ABI item {required}")
        else:
            if "$ientry_thunk$" in symbols:
                sys.exit("x64: unexpectedly emitted an ARM64EC hybrid thunk")
        print(f"PASS: {triple}: {expected} delegates to RtlUnwind with correct COFF ABI")

print("PASS: MSVCRT _local_unwind ARM64EC export/implementation parity")
