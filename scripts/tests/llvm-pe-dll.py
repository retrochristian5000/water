#!/usr/bin/env python3
"""Cross-link PE DLLs, DirectShow .ax filters and DLL consumers with Water's LLVM.

Usage: python3 scripts/tests/llvm-pe-dll.py /path/to/llvm/bin [--arch x86] [--lto thin]
This is a format/link regression, not a Windows runtime or COM registration test.
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


AX_SOURCE = """\
/* Exercise stdcall/x86 decoration while exporting the COM entry points undecorated. */
int __stdcall DllGetClassObject(void *clsid, void *iid, void **out)
{
    if (out) *out = 0;
    return 0;
}
int __stdcall DllCanUnloadNow(void) { return 0; }
int __stdcall DllRegisterServer(void) { return 0; }
int __stdcall DllUnregisterServer(void) { return 0; }
"""
AX_DEF_SOURCE = """\
LIBRARY water_probe.ax
EXPORTS
    DllGetClassObject
    DllCanUnloadNow
    DllRegisterServer
    DllUnregisterServer
"""
AX_EXPORTS = {"DllGetClassObject", "DllCanUnloadNow", "DllRegisterServer", "DllUnregisterServer"}

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


def check_ax(path, machine, magic):
    """Check that a .ax filter is a PE DLL with the intended module/export ABI."""
    if path.suffix != ".ax":
        raise AssertionError(f"{path}: DirectShow module must retain .ax suffix")
    check_pe(path, machine, magic, dll=True)
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    opt = pe + 24
    section_count = struct.unpack_from("<H", data, pe + 6)[0]
    section_table = opt + struct.unpack_from("<H", data, pe + 20)[0]
    export_rva, export_size = struct.unpack_from("<II", data, opt + (96 if magic == 0x10b else 112))
    if export_size < 40:
        raise AssertionError(f"{path}: missing PE export directory")

    def mapped(rva, size):
        for i in range(section_count):
            off = section_table + i * 40
            if off + 40 > len(data):
                raise AssertionError(f"{path}: truncated section table")
            virtual, raw_size, raw = (struct.unpack_from("<I", data, off + 12)[0],
                                      struct.unpack_from("<I", data, off + 16)[0],
                                      struct.unpack_from("<I", data, off + 20)[0])
            if virtual <= rva and rva - virtual + size <= raw_size:
                target = raw + rva - virtual
                if target + size <= len(data):
                    return target
        raise AssertionError(f"{path}: invalid export RVA {rva:#x}")

    def asciiz(rva):
        offset = mapped(rva, 1)
        end = data.find(b"\0", offset, min(len(data), offset + 256))
        if end == -1:
            raise AssertionError(f"{path}: unterminated export name")
        return data[offset:end].decode("ascii")

    directory = mapped(export_rva, 40)
    if asciiz(struct.unpack_from("<I", data, directory + 12)[0]) != path.name:
        raise AssertionError(f"{path}: PE export DLL name must match .ax output name")
    count = struct.unpack_from("<I", data, directory + 24)[0]
    if count != len(AX_EXPORTS):
        raise AssertionError(f"{path}: expected {len(AX_EXPORTS)} named COM exports, got {count}")
    names_rva = struct.unpack_from("<I", data, directory + 32)[0]
    table = mapped(names_rva, count * 4)
    names = {asciiz(struct.unpack_from("<I", data, table + i * 4)[0]) for i in range(count)}
    if names != AX_EXPORTS:
        raise AssertionError(f"{path}: wrong COM exports: {sorted(names)}")


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
        (root / "ax.c").write_text(AX_SOURCE, encoding="utf-8")
        (root / "ax.def").write_text(AX_DEF_SOURCE, encoding="utf-8")
        for arch in args.arch or TARGETS:
            triple, machine, magic = TARGETS[arch]
            path = root / arch
            path.mkdir()
            lto = [] if args.lto == "none" else [f"-flto={args.lto}"]
            for src, obj in (("dll.c", "dll.obj"), ("client.c", "client.obj"), ("ax.c", "ax.obj")):
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
            invoke(*link_command, "/nologo", "/dll", "/noentry", "/nodefaultlib",
                   f"/machine:{arch}", "/subsystem:windows", f"/out:{path / 'water_probe.ax'}",
                   f"/implib:{path / 'water_probe_ax.lib'}", f"/def:{root / 'ax.def'}", str(path / "ax.obj"))
            if not (path / "water_probe_ax.lib").read_bytes().startswith(b"!<arch>\n"):
                raise AssertionError(f"{arch}: missing AX COFF import library")
            check_ax(path / "water_probe.ax", machine, magic)
            print(f"PASS: {arch} {args.lto} LTO; DLL, EXE, AX export names and stdcall COM ABI")


if __name__ == "__main__":
    main()
