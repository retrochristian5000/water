#!/usr/bin/env python3
"""Inspect a Windows 9x MZ/LE VxD without executing or installing it.

No VxD control protocol is inferred from the filename or a resident name.
This utility is deliberately read-only. Use --expect-size to distinguish the
Windows 98 FE CWBMIDI.VXD (14513 bytes) from other Crystal driver releases.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


class VxdError(ValueError):
    pass


def u16(data, pos):
    if pos < 0 or pos + 2 > len(data):
        raise VxdError("truncated 16-bit field")
    return struct.unpack_from("<H", data, pos)[0]


def u32(data, pos):
    if pos < 0 or pos + 4 > len(data):
        raise VxdError("truncated 32-bit field")
    return struct.unpack_from("<I", data, pos)[0]


def resident_names(data, header, table, limit):
    if not table:
        return []
    if table < 0xac or table >= limit or header + limit > len(data):
        raise VxdError("invalid LE resident-name table offset")
    pos = table
    names = []
    for _ in range(256):
        length = data[header + pos]
        pos += 1
        if not length:
            return names
        if pos + length + 2 > limit:
            raise VxdError("truncated LE resident-name entry")
        name = data[header + pos:header + pos + length].decode("latin-1")
        ordinal = u16(data, header + pos + length)
        names.append({"name": name, "ordinal": ordinal})
        pos += length + 2
        if pos >= limit:
            break
    raise VxdError("unterminated or oversized LE resident-name table")


def inspect(data):
    if len(data) < 64 or data[:2] != b"MZ":
        raise VxdError("expected MZ DOS header")
    header = u32(data, 0x3c)
    if header < 64 or header > len(data) - 0xac:
        raise VxdError("LE header outside input file")
    if data[header:header + 2] != b"LE":
        raise VxdError("expected Windows 9x LE image; PE, NE and LX are not VxD LE")
    if data[header + 2] != 0 or data[header + 3] != 0:
        raise VxdError("unsupported LE byte or word ordering")
    # Every field below is relative to the beginning of the LE header.
    count = u32(data, header + 0x44)
    objects = u32(data, header + 0x40)
    if count > 4096 or (count and
                        (objects < 0xac or objects > len(data) - header or
                         count * 24 > len(data) - header - objects)):
        raise VxdError("invalid LE object table")
    table = u32(data, header + 0x58)
    relative_size = len(data) - header
    # Limit resident-name parsing to the next known LE table instead of
    # interpreting page data or fixups as names.
    next_tables = [u32(data, header + off) for off in
                   (0x40, 0x48, 0x4c, 0x50, 0x5c, 0x60, 0x68,
                    0x6c, 0x70, 0x78, 0x7c)]
    limit = min((v for v in next_tables if table and table < v <= relative_size),
                default=relative_size)
    return {
        "format": "MZ/LE",
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "le_header_offset": header,
        "cpu_type": u16(data, header + 0x08),
        "os_type": u16(data, header + 0x0a),
        "module_flags": u32(data, header + 0x10),
        "object_count": count,
        "resident_names": resident_names(data, header, table, limit),
        "vxd_device_id": None,  # DDB discovery requires a separate, verified decoder.
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file", type=Path, help="read-only VxD image to inspect")
    parser.add_argument("--expect-size", type=int, help="fail on size mismatch")
    parser.add_argument("--expect-name", help="require name in LE resident-name table")
    args = parser.parse_args(argv)
    try:
        data = args.file.read_bytes()
        result = inspect(data)
        if args.expect_size is not None and result["size"] != args.expect_size:
            raise VxdError("size mismatch: expected %d, got %d" %
                           (args.expect_size, result["size"]))
        if args.expect_name and not any(entry["name"].casefold() ==
                                        args.expect_name.casefold()
                                        for entry in result["resident_names"]):
            raise VxdError("requested name not present in LE resident-name table "
                           "(the VxD DDB may have a separate name)")
    except (OSError, VxdError) as exc:
        parser.exit(2, "inspect-vxd: %s\n" % exc)
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    sys.exit(main())
