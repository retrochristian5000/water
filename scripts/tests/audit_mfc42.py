#!/usr/bin/env python3
"""Inspect a separately supplied MFC42/MFC42U binary for Win9x x86 ABI triage.

Usage: python3 scripts/tests/audit_mfc42.py /path/to/mfc42.dll

This is a file-format/import/export audit, not MFC compatibility validation.
No Microsoft binary is bundled, downloaded or redistributed.
"""

import argparse
import struct
from pathlib import Path


class InvalidPE(ValueError):
    pass


def inspect(path):
    data = path.read_bytes()

    def field(fmt, offset):
        size = struct.calcsize(fmt)
        if offset < 0 or offset + size > len(data):
            raise InvalidPE(f"truncated field at file offset {offset:#x}")
        return struct.unpack_from(fmt, data, offset)

    if path.name.lower() not in ("mfc42.dll", "mfc42u.dll"):
        raise InvalidPE("expected mfc42.dll or mfc42u.dll (distinct ANSI/Unicode runtimes)")
    if data[:2] != b"MZ":
        raise InvalidPE("missing MZ header")
    pe_offset, = field("<I", 0x3c)
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise InvalidPE("missing PE signature")
    machine, sections, _, _, _, optional_size, characteristics = field("<HHIIIHH", pe_offset + 4)
    optional = pe_offset + 24
    if optional + optional_size > len(data) or optional_size < 112:
        raise InvalidPE("truncated PE32 optional header")
    magic, = field("<H", optional)
    if (machine, magic) != (0x014c, 0x010b) or not characteristics & 0x2000:
        raise InvalidPE("Win9x/VC6 MFC42 needs an i386 PE32 DLL, not PE32+ or NE")
    directory_count, = field("<I", optional + 92)
    if directory_count < 2:
        raise InvalidPE("missing export/import directory entries")
    export_rva, export_size, import_rva, import_size = field("<IIII", optional + 96)
    if not export_rva or export_size < 40:
        raise InvalidPE("missing PE export directory")
    size_of_headers, = field("<I", optional + 60)
    section_table = optional + optional_size
    if sections > 96 or section_table + sections * 40 > len(data):
        raise InvalidPE("invalid section table")
    ranges = []
    for i in range(sections):
        off = section_table + 40 * i
        virtual_size, rva, raw_size, raw_offset = field("<IIII", off + 8)
        if raw_size and raw_offset + raw_size > len(data):
            raise InvalidPE("section raw data exceeds file")
        ranges.append((rva, max(virtual_size, raw_size), raw_size, raw_offset))

    def at_rva(rva, size):
        if size < 0:
            raise InvalidPE("negative RVA span")
        if rva < size_of_headers and rva + size <= min(size_of_headers, len(data)):
            return rva
        for base, span, raw_size, raw_offset in ranges:
            relative = rva - base
            if relative >= 0 and relative + size <= min(span, raw_size):
                return raw_offset + relative
        raise InvalidPE(f"unmapped/truncated RVA {rva:#x} (+{size})")

    def string(rva):
        off = at_rva(rva, 1)
        # Each byte must remain mapped, not simply within the file.
        buf = bytearray()
        for i in range(1024):
            b = data[at_rva(rva + i, 1)]
            if not b:
                return buf.decode("ascii")
            buf.append(b)
        raise InvalidPE("unterminated or oversized PE string")

    exp = at_rva(export_rva, 40)
    dll_name_rva, = field("<I", exp + 12)
    export_name = string(dll_name_rva)
    if export_name.lower() != path.name.lower():
        raise InvalidPE(f"export directory identity {export_name!r} != {path.name!r}")
    export_functions, named_count = field("<II", exp + 20)
    if not export_functions or named_count > export_functions or named_count > 65536:
        raise InvalidPE("implausible PE export counts")
    export_names_rva, = field("<I", exp + 32)
    names = []
    if named_count:
        names_offset = at_rva(export_names_rva, named_count * 4)
        for i in range(named_count):
            name_rva, = field("<I", names_offset + i * 4)
            names.append(string(name_rva))
        if len(set(names)) != len(names):
            raise InvalidPE("duplicate named exports")

    imports = []
    if import_rva:
        if import_size < 20:
            raise InvalidPE("truncated PE import directory")
        for i in range(min(import_size // 20, 4096)):
            descriptor = at_rva(import_rva + 20 * i, 20)
            fields = field("<IIIII", descriptor)
            if fields == (0, 0, 0, 0, 0):
                break
            if not fields[3]:
                raise InvalidPE("import descriptor lacks a DLL name")
            imports.append(string(fields[3]))
        else:
            raise InvalidPE("unterminated PE import directory")

    return {
        "dll": path.name,
        "machine": "i386 (0x014c)",
        "format": "PE32 DLL",
        "exports": export_functions,
        "named_exports": named_count,
        "cxx_decorated_exports": sum(name.startswith("?") for name in names),
        "imports": imports,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path, help="existing native MFC42.DLL or MFC42U.DLL")
    args = parser.parse_args()
    try:
        report = inspect(args.dll)
    except (InvalidPE, OSError, UnicodeError, struct.error) as exc:
        parser.exit(1, f"FAIL: {args.dll}: {exc}\n")
    for key, value in report.items():
        print(f"{key}: {', '.join(value) if isinstance(value, list) else value}")
    if not report["cxx_decorated_exports"]:
        print("WARNING: no MSVC C++ decorated exports; MFC class identity is unconfirmed")
    if not report["imports"]:
        print("WARNING: no import DLLs detected; runtime dependency coverage is unconfirmed")
    print("FORMAT PASS only; loading, MFC class behavior and CRT exceptions remain untested")


if __name__ == "__main__":
    main()
