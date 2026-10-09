#!/usr/bin/env python3
"""Cross-link PE DLLs and DLL consumers with Water's matching LLVM tools.

Usage: python3 scripts/tests/llvm-pe-dll.py /path/to/llvm/bin [--arch x86] [--lto thin]
This is a format/link regression, not a Windows runtime test.
"""

import argparse
import struct
import subprocess
import tempfile
from pathlib import Path


TARGETS = {
    "x86": ("i686-pc-windows-msvc", 0x014C, 0x010B),
    "x64": ("x86_64-pc-windows-msvc", 0x8664, 0x020B),
    "arm64": ("aarch64-pc-windows-msvc", 0xAA64, 0x020B),
}

DLL_SOURCE = """\
__declspec(dllexport) int whp_answer(int x) { return x + 1; }
int whp_ordinal(int x) { return x + 3; }
"""
CLIENT_SOURCE = """\
__declspec(dllimport) int whp_answer(int);
__declspec(dllimport) int whp_ordinal(int);
int main(void) { return whp_answer(4) + whp_ordinal(5); }
"""
DEF_SOURCE = """\
LIBRARY whp_probe
EXPORTS
    whp_ordinal @17 NONAME
"""


def invoke(*args):
    try:
        subprocess.run(args, check=True, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, text=True)
    except subprocess.CalledProcessError as exc:
        raise RuntimeError(f"{' '.join(map(str, args))}\n{exc.stdout}{exc.stderr}") from exc


def check_pe(path, expected_machine, expected_magic, dll):
    data = path.read_bytes()
    if len(data) < 256 or data[:2] != b"MZ":
        raise AssertionError(f"{path}: missing DOS/PE header")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise AssertionError(f"{path}: missing PE signature")
    machine, flags = (struct.unpack_from("<H", data, pe + 4)[0],
                      struct.unpack_from("<H", data, pe + 22)[0])
    opt = pe + 24
    magic, entry = struct.unpack_from("<H", data, opt)[0], struct.unpack_from("<I", data, opt + 16)[0]
    directories = opt + (96 if magic == 0x010B else 112)
    exports, imports = struct.unpack_from("<II", data, directories)[0], struct.unpack_from("<II", data, directories + 8)[0]
    if (machine, magic) != (expected_machine, expected_magic):
        raise AssertionError(f"{path}: unexpected machine/PE class: {machine:#x}, {magic:#x}")
    if bool(flags & 0x2000) != dll:
        raise AssertionError(f"{path}: IMAGE_FILE_DLL does not match output type")
    if dll and (not exports or entry):
        raise AssertionError(f"{path}: DLL should export symbols and honor /NOENTRY")
    if not dll and (not imports or not entry):
        raise AssertionError(f"{path}: EXE should import from DLL and have an entry point")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("llvm_bin", type=Path, help="directory containing Water's clang and lld-link")
    parser.add_argument("--arch", choices=TARGETS, action="append", help="repeat to test specific target architectures")
    parser.add_argument("--lto", choices=("none", "thin", "full"), default="none")
    args = parser.parse_args()
    clang = args.llvm_bin / "clang"
    linker = args.llvm_bin / "lld-link"
    link_command = [str(linker)] if linker.is_file() else [str(args.llvm_bin / "lld"), "-flavor", "link"]
    if not clang.is_file() or not Path(link_command[0]).is_file():
        parser.error("clang and lld-link (or lld) must exist in the supplied LLVM bin directory")

    with tempfile.TemporaryDirectory(prefix="water-pe-dll-") as work:
        root = Path(work)
        (root / "dll.c").write_text(DLL_SOURCE, encoding="utf-8")
        (root / "client.c").write_text(CLIENT_SOURCE, encoding="utf-8")
        (root / "dll.def").write_text(DEF_SOURCE, encoding="utf-8")
        for arch in args.arch or TARGETS:
            triple, machine, magic = TARGETS[arch]
            path = root / arch
            path.mkdir()
            lto = [] if args.lto == "none" else [f"-flto={args.lto}"]
            for src, obj in (("dll.c", "dll.obj"), ("client.c", "client.obj")):
                invoke(str(clang), f"--target={triple}", "-ffreestanding",
                       "-fno-stack-protector", *lto, "-c", str(root / src), "-o", str(path / obj))
            invoke(*link_command, "/nologo", "/dll", "/noentry", "/nodefaultlib",
                   f"/machine:{arch}", f"/out:{path / 'whp.dll'}",
                   f"/implib:{path / 'whp.lib'}", f"/def:{root / 'dll.def'}", str(path / "dll.obj"))
            if not (path / "whp.lib").read_bytes().startswith(b"!<arch>\n"):
                raise AssertionError(f"{arch}: missing COFF import library")
            invoke(*link_command, "/nologo", "/nodefaultlib", f"/machine:{arch}",
                   "/entry:main", "/subsystem:console", f"/out:{path / 'client.exe'}",
                   str(path / "client.obj"), str(path / "whp.lib"))
            check_pe(path / "whp.dll", machine, magic, dll=True)
            check_pe(path / "client.exe", machine, magic, dll=False)
            print(f"PASS: {arch} {args.lto} LTO; DLL exports, ordinal import, EXE imports, PE headers")


if __name__ == "__main__":
    main()
