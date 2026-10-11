#!/usr/bin/env python3
"""Audit direct PE imports of Jet Red/Blue in extracted Windows binaries.

No executable is launched and no DLL is loaded. Does not inspect indirect
COM activation, runtime LoadLibrary calls, or packed/NE/LX binaries.
"""
import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

JET_RED = {"msjet35.dll", "msjet40.dll", "msjetoledb40.dll", "msjtor35.dll",
           "msjt4jlt.dll", "dao350.dll", "dao360.dll"}
JET_BLUE = {"esent.dll"}
EXTENSIONS = {".exe", ".dll", ".ocx", ".cpl", ".drv"}


class PEError(ValueError):
    pass


def get(data, off, fmt):
    size = struct.calcsize(fmt)
    if off < 0 or off + size > len(data):
        raise PEError("truncated PE field")
    return struct.unpack_from("<" + fmt, data, off)[0]


def inspect(path):
    data = path.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise PEError("not an MZ executable")
    pe = get(data, 0x3c, "I")
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        raise PEError("not a PE image (possibly NE/LE/LX)")
    coff = pe + 4
    machine = get(data, coff, "H")
    nsections = get(data, coff + 2, "H")
    opt_size = get(data, coff + 16, "H")
    opt = coff + 20
    if not 1 <= nsections <= 96 or opt + opt_size > len(data):
        raise PEError("invalid PE section table")
    magic = get(data, opt, "H")
    if magic == 0x10b:
        dd, image_base = 96, get(data, opt + 28, "I")
        count_off = 92
    elif magic == 0x20b:
        dd, image_base = 112, get(data, opt + 24, "Q")
        count_off = 108
    else:
        raise PEError("unsupported PE optional header")
    if opt_size < dd or opt_size < count_off + 4:
        raise PEError("short PE optional header")
    directory_count = get(data, opt + count_off, "I")
    sections = []
    for i in range(nsections):
        s = opt + opt_size + i * 40
        if s + 40 > len(data):
            raise PEError("truncated PE sections")
        virtual_size = get(data, s + 8, "I")
        rva = get(data, s + 12, "I")
        raw_size = get(data, s + 16, "I")
        raw = get(data, s + 20, "I")
        sections.append((rva, virtual_size, raw_size, raw))

    size_of_headers = get(data, opt + 60, "I")

    def offset(rva, size):
        if rva < size_of_headers and rva + size <= min(size_of_headers, len(data)):
            return rva
        for start, virtual, raw_size, raw in sections:
            # Only initialized bytes are addressable in the file.
            if rva >= start and rva - start + size <= raw_size and rva - start < max(virtual, raw_size):
                pos = raw + (rva - start)
                if pos + size <= len(data):
                    return pos
        raise PEError("unmapped PE RVA")

    def cstring(rva):
        pos = offset(rva, 1)
        end = data.find(b"\0", pos, min(len(data), pos + 512))
        if end < 0:
            raise PEError("unterminated import DLL name")
        try:
            return data[pos:end].decode("ascii").lower()
        except UnicodeDecodeError as exc:
            raise PEError("non-ASCII import DLL name") from exc

    def directory(index):
        if directory_count <= index or opt_size < dd + 8 * (index + 1):
            return (0, 0)
        return (get(data, opt + dd + 8 * index, "I"),
                get(data, opt + dd + 8 * index + 4, "I"))

    def imports(index, width, name_idx):
        rva, length = directory(index)
        if not rva or not length:
            return []
        if length < width:
            raise PEError("short import descriptor directory")
        found = []
        # Require a zero descriptor within the declared directory bounds.
        for i in range(min(length // width, 4096)):
            start = offset(rva + i * width, width)
            desc = data[start:start + width]
            if desc == bytes(width):
                return found
            name = get(data, start + name_idx, "I")
            if index == 13 and not (get(data, start, "I") & 1):
                # Delay import descriptors may use VAs instead of RVAs.
                if name < image_base:
                    raise PEError("invalid VA in delay import descriptor")
                name -= image_base
            found.append(cstring(name))
        raise PEError("unterminated import descriptor directory")

    normal = imports(1, 20, 12)
    delay = imports(13, 32, 4)
    deps = set(normal + delay)
    return {
        "file": str(path), "sha256": hashlib.sha256(data).hexdigest(),
        "machine": f"0x{machine:04x}",
        "format": "PE32" if magic == 0x10b else "PE32+",
        "imports": normal, "delay_imports": delay,
        "jet_red": sorted(deps & JET_RED),
        "jet_blue": sorted(deps & JET_BLUE),
        "jet_red_module": path.name.lower() in JET_RED,
        "jet_blue_module": path.name.lower() in JET_BLUE,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", type=Path, nargs="+", help="PE files or extracted Windows directories")
    parser.add_argument("--all", action="store_true", help="include unrelated PE files")
    parser.add_argument("--json", action="store_true", help="emit machine-readable provenance")
    args = parser.parse_args(argv)
    paths = []
    for path in args.paths:
        if path.is_file():
            paths.append(path)
        elif path.is_dir():
            paths.extend(p for p in path.rglob("*") if p.is_file() and p.suffix.lower() in EXTENSIONS)
        else:
            parser.error(f"missing input: {path}")
    paths = sorted(set(paths))
    results = []
    skipped = []
    for path in paths:
        try:
            result = inspect(path)
        except (PEError, OSError) as exc:
            skipped.append({"file": str(path), "reason": str(exc)})
            continue
        if args.all or result["jet_red"] or result["jet_blue"] or result["jet_red_module"] or result["jet_blue_module"]:
            results.append(result)
    output = {"scanned": len(paths), "matches": results, "skipped": skipped,
              "note": "Direct/delay imports only; COM, LoadLibrary and transitive dependencies unproven"}
    if args.json:
        print(json.dumps(output, indent=2))
    else:
        print(f"PE files considered: {len(paths)}; JET matches: {len(results)}; skipped: {len(skipped)}")
        for result in results:
            print(f"{result['file']} [{result['format']} {result['machine']}]: "
                  f"Red={result['jet_red']} Blue={result['jet_blue']} "
                  f"engine-file={result['jet_red_module'] or result['jet_blue_module']}")
        if skipped:
            print("Skipped non-PE/unreadable files (use --json for details)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
