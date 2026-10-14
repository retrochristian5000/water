#!/usr/bin/env python3
"""Exercise the exact Water and LLVM ARM64EC shift fallback bodies on the host.

The separate arm64-winheaders-abi.sh checks real header resolution and target IR.
This test verifies both low/high semantics for all unsigned-char shift counts.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SOURCES = (
    ROOT / "include/msvcrt/intrin.h",
    ROOT / "toolchains/llvm-project/clang/lib/Headers/intrin0.h",
)
NAMES = ("__shiftright128", "__shiftleft128")


def extract(text, name, prefix):
    pattern = (
        r"static\s+__inline__\s+unsigned\s+__int64\s+"
        + re.escape(name)
        + r"\s*\([^)]*\)\s*\{[^{}]*\}"
    )
    matches = re.findall(pattern, text, flags=re.DOTALL)
    if len(matches) != 1:
        raise AssertionError(f"expected one ARM64EC fallback for {name}, got {len(matches)}")
    return matches[0].replace(name + "(", prefix + name + "(", 1)


def main():
    cc = os.environ.get("WHP_HEADER_CC", "clang")
    functions = []
    for index, path in enumerate(SOURCES):
        if not path.is_file():
            raise AssertionError(f"missing tested header (initialize Water submodules): {path}")
        text = path.read_text(encoding="utf-8")
        for name in NAMES:
            functions.append(extract(text, name, f"provider{index}_"))

    source = (
        "#include <stdint.h>\n#include <stdio.h>\n"
        + "\n".join(functions)
        + r"""
static uint64_t ref_right(uint64_t low, uint64_t high, unsigned char n)
{
    unsigned s = n & 63;
    return s ? ((low >> s) | (high << (64 - s))) : low;
}
static uint64_t ref_left(uint64_t low, uint64_t high, unsigned char n)
{
    unsigned s = n & 63;
    return s ? ((high << s) | (low >> (64 - s))) : high;
}
int main(void)
{
    static const uint64_t v[] = {
        0, UINT64_MAX, UINT64_C(0x8000000000000000),
        UINT64_C(0x0123456789abcdef), UINT64_C(0xfedcba9876543210)
    };
    for (unsigned a = 0; a < sizeof v / sizeof v[0]; a++)
        for (unsigned b = 0; b < sizeof v / sizeof v[0]; b++)
            for (unsigned n = 0; n < 256; n++)
            {
                uint64_t low = v[a], high = v[b];
                uint64_t right = ref_right(low, high, (unsigned char)n);
                uint64_t left = ref_left(low, high, (unsigned char)n);
                if (provider0___shiftright128(low, high, n) != right ||
                    provider1___shiftright128(low, high, n) != right ||
                    provider0___shiftleft128(low, high, n) != left ||
                    provider1___shiftleft128(low, high, n) != left)
                {
                    fprintf(stderr, "shift mismatch: a=%u b=%u count=%u\n", a, b, n);
                    return 1;
                }
            }
    return 0;
}
"""
    )
    with tempfile.TemporaryDirectory(prefix="water-arm64ec-shifts-") as tmp:
        src = Path(tmp) / "shift.c"
        exe = Path(tmp) / "shift"
        src.write_text(source, encoding="utf-8")
        subprocess.run([cc, "-std=c11", "-fms-extensions", "-O2", "-Wall",
                        "-Wextra", str(src), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True)
    print("PASS: exact Water/LLVM shift fallbacks, 256 counts x 25 value pairs")


if __name__ == "__main__":
    main()
