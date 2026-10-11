#!/usr/bin/env python3
"""Verify clang-cl /F reaches the Windows executable's PE stack-reserve header.

Usage: python3 scripts/tests/llvm-pe-stack.py /path/to/llvm/bin
Requires the matching rebuilt clang-cl and lld-link. Does not run Windows code.
"""
import argparse
import os
import struct
import subprocess
import tempfile
from pathlib import Path

TARGETS = {
    "x86": ("i686-pc-windows-msvc", 0x014c, 0x010b),
    "x64": ("x86_64-pc-windows-msvc", 0x8664, 0x020b),
    "arm64": ("aarch64-pc-windows-msvc", 0xaa64, 0x020b),
}


def run(args, env, success=True):
    p = subprocess.run(list(map(str, args)), env=env, capture_output=True, text=True)
    if (p.returncode == 0) != success:
        raise AssertionError(f"exit {p.returncode}: {' '.join(map(str, args))}\n{p.stdout}{p.stderr}")
    return p


def stack_fields(path, machine, magic):
    data = path.read_bytes()
    assert data[:2] == b"MZ", f"missing DOS header: {path}"
    nt = struct.unpack_from("<I", data, 0x3c)[0]
    assert data[nt:nt + 4] == b"PE\0\0"
    got_machine = struct.unpack_from("<H", data, nt + 4)[0]
    optional = nt + 24
    got_magic = struct.unpack_from("<H", data, optional)[0]
    flags = struct.unpack_from("<H", data, nt + 22)[0]
    assert (got_machine, got_magic) == (machine, magic)
    assert not flags & 0x2000, "expected executable, not DLL"
    fmt = "<I" if magic == 0x010b else "<Q"
    offset = struct.calcsize(fmt)
    return (struct.unpack_from(fmt, data, optional + 72)[0],
            struct.unpack_from(fmt, data, optional + 72 + offset)[0])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("llvm_bin", type=Path)
    parser.add_argument("--arch", choices=TARGETS, action="append")
    opt = parser.parse_args()
    bin_dir = opt.llvm_bin.resolve()
    clang = bin_dir / "clang-cl"
    linker = bin_dir / "lld-link"
    if not clang.is_file() or not linker.is_file():
        parser.error("matching clang-cl and lld-link binaries are required")
    env = dict(os.environ, PATH=str(bin_dir) + os.pathsep + os.environ.get("PATH", ""))

    with tempfile.TemporaryDirectory(prefix="water-f-stack-") as work:
        root = Path(work)
        source = root / "main.c"
        source.write_text("int main(void) { return 0; }\n")
        for arch in opt.arch or TARGETS:
            triple, machine, magic = TARGETS[arch]
            work_dir = root / arch
            work_dir.mkdir()

            def linked(name, opts=(), link_opts=(), success=True):
                target = work_dir / (name + ".exe")
                cmd = [clang, "--target=" + triple, "/GS-", "-fuse-ld=lld",
                       "/Tc" + str(source), "/Fe:" + str(target),
                       *opts, "/link", "/nodefaultlib", "/entry:main",
                       "/subsystem:console", *link_opts]
                run(cmd, env, success)
                return stack_fields(target, machine, magic) if success else None

            cases = [
                ("default", [], 1048576),
                ("joined", ["/F5"], 8),
                ("separate", ["/F", "010"], 8),
                ("hex", ["/F0x2001"], 8196),
                ("last", ["/F5", "/F9"], 12),
            ]
            for name, opts, reserve in cases:
                actual, commit = linked(name, opts)
                assert (actual, commit) == (reserve, 4096), (arch, name, actual, commit)
            actual, commit = linked("override", ["/F5"], ["/STACK:65536"])
            assert (actual, commit) == (65536, 4096), (arch, "override", actual, commit)

            for name, flag in (("zero", "/F0"), ("invalid", "/Fnotanumber")):
                linked(name, [flag], success=False)
            if arch == "x86":
                linked("x86-overflow", ["/F4294967293"], success=False)
                # Also catch direct LLD /STACK overflow, not just /F.
                obj = work_dir / "raw.obj"
                run([clang, "--target=" + triple, "/GS-", "/c",
                     "/Tc" + str(source), "/Fo:" + str(obj)], env)
                run([linker, "/nologo", "/nodefaultlib", "/subsystem:console",
                     "/entry:main", "/stack:4294967297",
                     "/out:" + str(work_dir / "overflow.exe"), obj], env,
                    success=False)
            else:
                actual, commit = linked("pe32plus-wide", ["/F4294967293"])
                assert (actual, commit) == (4294967296, 4096)
            print(f"PASS: {arch}, /F numeric forms, rounding, linker precedence and PE stack fields")


if __name__ == "__main__":
    main()
