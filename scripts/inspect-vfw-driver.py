#!/usr/bin/env python3
"""Read-only PE export inspector for Win9x Video for Windows installable drivers.

Inspect a lawfully obtained VDOWAVE.DRV without loading or executing it.
A VfW codec typically exports DriverProc; the codec FOURCC is supplied by
[drivers32] registration and cannot be inferred from the filename or export.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


class DriverFormatError(ValueError):
    pass


def integer(data, off, size):
    if off < 0 or off + size > len(data):
        raise DriverFormatError("truncated PE field")
    return int.from_bytes(data[off:off + size], "little")


def cstring(data, off, limit=4096):
    if off < 0 or off >= len(data):
        raise DriverFormatError("string outside PE image")
    end = data.find(b"\0", off, min(len(data), off + limit))
    if end < 0:
        raise DriverFormatError("unterminated PE export name")
    return data[off:end].decode("ascii", errors="replace")


def inspect(data):
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise DriverFormatError("expected MZ DOS header")
    pos = integer(data, 0x3c, 4)
    if pos < 0x40 or pos + 4 > len(data):
        raise DriverFormatError("invalid new executable header offset")
    if data[pos:pos + 2] == b"NE":
        raise DriverFormatError("NE/Win16 driver, not a PE32 Video for Windows codec")
    if data[pos:pos + 4] != b"PE\0\0":
        raise DriverFormatError("expected PE image; unsupported driver format")
    coff = pos + 4
    machine = integer(data, coff, 2)
    nsections = integer(data, coff + 2, 2)
    opt_size = integer(data, coff + 16, 2)
    characteristics = integer(data, coff + 18, 2)
    if not 1 <= nsections <= 96 or opt_size < 96:
        raise DriverFormatError("invalid COFF header")
    opt = coff + 20
    section_base = opt + opt_size
    if section_base > len(data) or nsections * 40 > len(data) - section_base:
        raise DriverFormatError("truncated PE section table")
    magic = integer(data, opt, 2)
    if magic == 0x10b:  # PE32
        ndirs = 92
        exports = 96
        bits = 32
    elif magic == 0x20b:  # PE32+
        ndirs = 108
        exports = 112
        bits = 64
    else:
        raise DriverFormatError("unrecognized PE optional-header magic")
    if opt_size < exports + 8:
        raise DriverFormatError("PE optional header too short for data directories")
    directory_count = integer(data, opt + ndirs, 4)
    export_rva = integer(data, opt + exports, 4) if directory_count else 0
    export_size = integer(data, opt + exports + 4, 4) if directory_count else 0
    if export_rva and not export_size:
        raise DriverFormatError("invalid PE export directory size")
    sections = []
    for i in range(nsections):
        s = section_base + i * 40
        sections.append((integer(data, s + 12, 4), integer(data, s + 8, 4),
                         integer(data, s + 16, 4), integer(data, s + 20, 4)))

    def rva_to_offset(rva, size):
        for virt, virtual_size, raw_size, raw in sections:
            span = max(virtual_size, raw_size)
            if rva < virt or rva - virt >= span:
                continue
            delta = rva - virt
            if size > raw_size - delta or delta > raw_size or raw > len(data) or size > len(data) - raw - delta:
                raise DriverFormatError("RVA references unmapped or truncated section bytes")
            return raw + delta
        raise DriverFormatError("unmapped PE export RVA")

    entries = []
    dll_name = None
    if export_rva:
        exp = rva_to_offset(export_rva, 40)
        base = integer(data, exp + 16, 4)
        funcs = integer(data, exp + 20, 4)
        count = integer(data, exp + 24, 4)
        if funcs > 65536 or count > funcs or count > 65536:
            raise DriverFormatError("invalid PE export counts")
        name_rva = integer(data, exp + 12, 4)
        if name_rva:
            name_off = rva_to_offset(name_rva, 1)
            dll_name = cstring(data, name_off)
        if count:
            fn_tab = rva_to_offset(integer(data, exp + 28, 4), funcs * 4)
            names = rva_to_offset(integer(data, exp + 32, 4), count * 4)
            ords = rva_to_offset(integer(data, exp + 36, 4), count * 2)
            for i in range(count):
                ordinal_index = integer(data, ords + 2 * i, 2)
                if ordinal_index >= funcs:
                    raise DriverFormatError("out-of-range PE export ordinal")
                func_rva = integer(data, fn_tab + ordinal_index * 4, 4)
                nrva = integer(data, names + i * 4, 4)
                noff = rva_to_offset(nrva, 1)
                name = cstring(data, noff)
                # Export-forwarder RVA entries are strings, not code.
                forwarder = None
                if export_rva <= func_rva < export_rva + export_size:
                    forwarder = cstring(data, rva_to_offset(func_rva, 1))
                entries.append({"name": name, "ordinal": base + ordinal_index,
                                "forwarder": forwarder})
    return {
        "format": "PE32" if bits == 32 else "PE32+",
        "machine": "i386" if machine == 0x14c else "amd64" if machine == 0x8664 else
                   "arm64" if machine == 0xaa64 else f"0x{machine:04x}",
        "dll_characteristic": bool(characteristics & 0x2000),
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "image_name": dll_name,
        "exports": entries,
        "has_DriverProc": any(e["name"] == "DriverProc" for e in entries),
        "codec_fourcc": None,  # Obtain from actual VfW configuration or codec implementation.
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path)
    parser.add_argument("--expect-size", type=int)
    parser.add_argument("--require-driverproc", action="store_true")
    args = parser.parse_args(argv)
    try:
        # Prevent accidental analysis of huge/corrupt disk images.
        if args.file.stat().st_size > 64 * 1024 * 1024:
            raise DriverFormatError("input larger than 64 MiB")
        record = inspect(args.file.read_bytes())
        if args.expect_size is not None and record["size"] != args.expect_size:
            raise DriverFormatError(f"size mismatch: expected {args.expect_size}, got {record['size']}")
        if args.require_driverproc and not record["has_DriverProc"]:
            raise DriverFormatError("DriverProc export not found")
    except (OSError, DriverFormatError) as exc:
        parser.exit(2, f"inspect-vfw-driver: {exc}\n")
    print(json.dumps(record, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
