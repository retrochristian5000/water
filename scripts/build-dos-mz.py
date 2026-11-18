#!/usr/bin/env python3
"""Package one self-contained .code16 i386 COFF object as a DOS MZ executable."""
import argparse
import os
from contextlib import suppress
from pathlib import Path
import struct
import tempfile


def load_sections(obj):
    if len(obj) < 20:
        raise ValueError("truncated COFF header")
    machine, count, _, _, _, optional, flags = struct.unpack_from("<HHIIIHH", obj)
    if machine != 0x14c or optional or flags & 2:
        raise ValueError("expected a relocatable i386 COFF object")
    table_end = 20 + count * 40
    if table_end > len(obj):
        raise ValueError("truncated COFF section table")

    sections = {}
    for i in range(count):
        name, _, _, size, start, _, _, relocs, _, attrs = struct.unpack_from(
            "<8sIIIIIIHHI", obj, 20 + i * 40)
        name = name.rstrip(b"\0")
        if relocs or attrs & 0x01000000:
            raise ValueError(f"unresolved COFF relocations in {name!r}")
        if name in (b".data", b".bss") and not size:
            continue
        if name not in (b".text", b".mzrel") or name in sections:
            raise ValueError(f"unsupported or duplicate section {name!r}")
        if name == b".text" and not attrs & 0x20:  # IMAGE_SCN_CNT_CODE
            raise ValueError(".text is not a COFF code section")
        if size and (start < table_end or start + size > len(obj)):
            raise ValueError(f"invalid data extent for {name!r}")
        sections[name] = obj[start:start + size]
    return sections


def build_mz(obj):
    sections = load_sections(obj)
    image = sections.get(b".text", b"")
    if not 0 < len(image) <= 65536:
        raise ValueError("expected 1 to 65536 load image bytes in .text")
    offsets = sections.get(b".mzrel", b"")
    if len(offsets) % 2:
        raise ValueError(".mzrel must contain 16-bit word offsets")
    table = bytearray()
    previous = -2
    for offset, in sorted(struct.iter_unpack("<H", offsets)):
        if offset + 2 > len(image) or offset < previous + 2:
            raise ValueError("overlapping or out-of-image segment fixup")
        table.extend(struct.pack("<HH", offset, 0))
        previous = offset

    # CS:IP = 0:0. SS starts on the first paragraph after the load image.
    # DOS supplies the 256-byte descending stack via e_minalloc.
    header_size = (28 + len(table) + 15) & ~15
    size = header_size + len(image)
    header = struct.pack("<14H", 0x5a4d, size % 512, (size + 511) // 512,
                         len(table) // 4, header_size // 16, 16, 16,
                         (len(image) + 15) // 16, 256, 0, 0, 0, 28, 0)
    return (header + table).ljust(header_size, b"\0") + image


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("object", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        if args.object.resolve() == args.output.resolve():
            raise ValueError("input and output paths must differ")
        exe = build_mz(args.object.read_bytes())
        tmp = tempfile.NamedTemporaryFile(dir=args.output.parent, delete=False)
        try:
            with tmp:
                tmp.write(exe)
            os.replace(tmp.name, args.output)
        finally:
            with suppress(FileNotFoundError):
                os.unlink(tmp.name)
    except (OSError, ValueError) as exc:
        parser.exit(1, f"dos-mz: {exc}\n")


if __name__ == "__main__":
    main()
