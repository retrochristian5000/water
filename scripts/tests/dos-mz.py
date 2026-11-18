#!/usr/bin/env python3
"""Check the public MZ build target. Optionally pass an LLVM-assembled mz-exit.obj."""
from pathlib import Path
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "scripts/tests/dos/mz-exit.s"


def build(source, output, *, check=True):
    return subprocess.run([str(ROOT / "build.sh"), "dos-mz", str(source), str(output)],
                          check=check, stderr=subprocess.PIPE, text=True)


def check_probe(exe):
    data = exe.read_bytes()
    (magic, last, pages, count, paragraphs, minimum, maximum, ss, sp, _, ip, cs,
     table, overlay) = struct.unpack_from("<14H", data)
    assert magic == 0x5a4d and (pages - 1) * 512 + (last or 512) == len(data)
    image = data[paragraphs * 16:]
    assert image and (cs, ip, overlay) == (0, 0, 0)
    assert len(image) <= ss * 16 and ss * 16 + sp <= ((len(image) + 15) // 16 + minimum) * 16
    assert maximum >= minimum and sp == 256
    assert count == 1 and 28 <= table <= paragraphs * 16 - 4
    offset, segment = struct.unpack_from("<HH", data, table)
    assert segment == 0 and offset + 2 <= len(image) and image[offset:offset + 2] == b"\0\0"


def text_object(payload, relocations=0):
    # Minimal COFF input for page-size and rejection tests.
    header = struct.pack("<HHIIIHH", 0x14c, 1, 0, 0, 0, 0, 0)
    section = struct.pack("<8sIIIIIIHHI", b".text", 0, 0, len(payload), 60,
                          0, 0, relocations, 0, 0x60000020)
    return header + section + payload


def main():
    with tempfile.TemporaryDirectory(prefix="water DOS ") as temp:
        directory = Path(temp)
        exe = directory / "probe.exe"
        build(Path(sys.argv[1]).resolve() if len(sys.argv) == 2 else PROBE, exe)
        check_probe(exe)
        obj = directory / "page.obj"
        for length, pages, tail in ((479, 1, 511), (480, 1, 0), (481, 2, 1)):
            payload = bytes([0x90]) * length
            obj.write_bytes(text_object(payload))
            build(obj, exe)
            data = exe.read_bytes()
            assert struct.unpack_from("<HH", data, 2) == (tail, pages)
            assert data[32:] == payload
        for invalid in (data, text_object(b"x", relocations=1), text_object(b"x")[:-1]):
            obj.write_bytes(invalid)
            assert build(obj, exe, check=False).returncode != 0
            assert exe.read_bytes() == data
    print("PASS: DOS MZ entry, stack, relocation, page boundaries and failed-build recovery")


if __name__ == "__main__":
    main()
