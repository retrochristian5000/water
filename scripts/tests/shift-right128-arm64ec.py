#!/usr/bin/env python3
"""Test Water's exact ShiftRight128 helper against MSVC x64 semantics.

This extracts the existing inline body from include/winnt.h, compiles and
executes it on the host for all 256 BYTE counts, and compiles the same body
for ARM64EC / native ARM64 Windows to check target codegen.  It is not a
Windows-on-Arm execution test, nor does it claim that the standalone
source replaces the complete winnt.h include-graph build.
"""
from pathlib import Path
import os
import re
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
HEADER = ROOT / "include" / "winnt.h"
CLANG = os.environ.get("WHP_SHIFT_CC", "clang")


def check(condition, message):
    if not condition:
        raise AssertionError(message)


def run(*args):
    subprocess.run(args, check=True)


def capture(*args):
    return subprocess.check_output(args, text=True)


def main():
    if not shutil.which(CLANG):
        print("SKIP: Clang compiler is unavailable", file=sys.stderr)
        return 77

    source = HEADER.read_text(encoding="utf-8")
    match = re.search(
        r"static FORCEINLINE DWORD64 ShiftRight128\s*\("
        r"\s*DWORD64 lo,\s*DWORD64 hi,\s*BYTE shift\s*\)"
        r"\s*\{.*?\n\}",
        source,
        flags=re.DOTALL,
    )
    check(match is not None, "cannot find Water's actual ShiftRight128 helper")
    # The ARM64EC target defines both __x86_64__ and __arm64ec__, but
    # must not emit a call to the genuine x64-only intrinsic.
    check(
        re.search(r"^\s*#\s*define\s+__shiftright128\s+ShiftRight128\s*$",
                  source, re.MULTILINE) is not None,
        "ARM64EC __shiftright128 compatibility mapping is missing",
    )

    helper = match.group(0).replace("static FORCEINLINE", "static inline")
    prelude = (
        "typedef unsigned long long DWORD64;\n"
        "typedef unsigned char BYTE;\n"
        + helper + "\n"
        "#define __shiftright128 ShiftRight128\n"
    )

    # Verify the original implementation, not a reimplementation of it.
    # Reference uses shifts <= 63 only to avoid C undefined behavior.
    runtime = r"""
#include <stdint.h>
#include <stdio.h>

static uint64_t expected(uint64_t lo, uint64_t hi, unsigned char count)
{
    unsigned s = count & 63;
    return s ? ((lo >> s) | (hi << (64 - s))) : lo;
}

int main(void)
{
    static const uint64_t values[] = {
        0, UINT64_MAX, UINT64_C(0x8000000000000000),
        UINT64_C(0x0123456789abcdef), UINT64_C(0xfedcba9876543210)
    };
    unsigned i, j, s;
    for (i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        for (j = 0; j < sizeof(values) / sizeof(values[0]); j++)
            for (s = 0; s < 256; s++)
            {
                uint64_t actual = __shiftright128(values[i], values[j], (BYTE)s);
                uint64_t correct = expected(values[i], values[j], (BYTE)s);
                if (actual != correct)
                {
                    fprintf(stderr, "lo=%llx hi=%llx shift=%u actual=%llx expected=%llx\n",
                            (unsigned long long)values[i],
                            (unsigned long long)values[j], s,
                            (unsigned long long)actual, (unsigned long long)correct);
                    return 1;
                }
            }
    return 0;
}
"""
    cross = (
        prelude
        + "\nDWORD64 water_shift_abi(DWORD64 lo, DWORD64 hi, BYTE n)\n"
        + "{ return __shiftright128(lo, hi, n); }\n"
    )
    with tempfile.TemporaryDirectory(prefix="water-shiftright128-") as tmp:
        tmpdir = Path(tmp)
        host_source = tmpdir / "shift-host.c"
        host_binary = tmpdir / "shift-host"
        host_source.write_text(prelude + runtime, encoding="utf-8")
        run(CLANG, "-std=c11", "-O2", "-Wall", "-Wextra",
            str(host_source), "-o", str(host_binary))
        run(str(host_binary))
        print("PASS: exact Water helper matches MSVC modulo-64 semantics (6400 cases)")

        cross_source = tmpdir / "shift-windows.c"
        cross_source.write_text(cross, encoding="utf-8")
        for triple in ("arm64ec-pc-windows-msvc", "aarch64-pc-windows-msvc"):
            ir = capture(
                CLANG, "-target", triple, "-ffreestanding", "-O2",
                "-S", "-emit-llvm", str(cross_source), "-o", "-"
            )
            check(
                "@water_shift_abi(" in ir and "define" in ir,
                f"{triple}: expected exported shift function",
            )
            check(
                re.search(r"and i8 [^,\n]+, 63", ir) is not None
                or "@llvm.fshr.i64(" in ir,
                f"{triple}: count masking/funnel-shift not preserved",
            )
            check(
                re.search(r"^declare .*@__shiftright128\(", ir, re.MULTILINE) is None,
                f"{triple}: unresolved x64-only intrinsic escaped ABI alias",
            )
            print(f"PASS: {triple} preserves 64-bit result and modulo-64 shift")

        # x64 is the documented native intrinsic. Clang lowers it to
        # llvm.fshr.i64, which is modulo 64 by LLVM definition.
        builtin_source = tmpdir / "shift-x64.c"
        builtin_source.write_text(
            "unsigned long long __shiftright128("
            "unsigned long long, unsigned long long, unsigned char);\n"
            "unsigned long long native_shift(unsigned long long low,"
            " unsigned long long high, unsigned char shift)\n"
            "{ return __shiftright128(low, high, shift); }\n",
            encoding="utf-8",
        )
        native = capture(
            CLANG, "-target", "x86_64-pc-windows-msvc", "-O2",
            "-S", "-emit-llvm", str(builtin_source), "-o", "-"
        )
        check("@llvm.fshr.i64(" in native, "x64 intrinsic funnel-shift changed")
        print("PASS: x64 MSVC intrinsic remains a native LLVM funnel shift")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (AssertionError, subprocess.CalledProcessError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
