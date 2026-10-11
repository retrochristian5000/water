#!/usr/bin/env python3
"""Read-only Windows CC decoder driver/INF inventory; never load a driver.

Finds original-media evidence for ccdecode.sys, ccdecode.inf and INF entries
referencing those names. No conclusion about bundled Windows version follows
from filenames alone. Does not decode CAB archives or enumerate live devices.
"""
import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path

MAX_BYTES = 32 * 1024 * 1024
REF = re.compile(r"ccdecode|closed[ \t_-]*caption[ \t_-]*decod", re.I)
SECTION = re.compile(r"^\s*\[([^\]]+)\]\s*$")


def pe_header(data):
    if len(data) < 64 or data[:2] != b"MZ":
        return {"format": "not-MZ"}
    pe = struct.unpack_from("<I", data, 0x3c)[0]
    if pe > len(data) - 26 or data[pe:pe + 4] != b"PE\0\0":
        return {"format": "MZ, not confirmed PE"}
    machine = struct.unpack_from("<H", data, pe + 4)[0]
    optional = struct.unpack_from("<H", data, pe + 24)[0]
    return {"format": {0x10b: "PE32", 0x20b: "PE32+"}.get(optional, "unknown PE"),
            "machine": "0x%04x" % machine}


def scan_file(path):
    if path.stat().st_size > MAX_BYTES:
        raise ValueError("file exceeds the read-only inventory size limit")
    data = path.read_bytes()
    name = path.name.lower()
    record = {"path": str(path), "bytes": len(data),
              "sha256": hashlib.sha256(data).hexdigest()}
    if name == "ccdecode.sys":
        record.update({"kind": "candidate-driver", **pe_header(data)})
        return record
    if path.suffix.lower() != ".inf":
        return None
    # Win9x INFs may use ANSI code pages. Keep original hashes and lines.
    body = data.decode("cp1252", errors="replace")
    section, hits = None, []
    for lineno, line in enumerate(body.splitlines(), 1):
        stripped = line.strip()
        match = SECTION.fullmatch(stripped)
        if match:
            section = match.group(1)
        if not stripped.startswith(";") and REF.search(line.split(";", 1)[0]):
            hits.append({"line": lineno, "section": section,
                         "text": line.rstrip()[:400]})
    if name != "ccdecode.inf" and not hits:
        return None
    record.update({"kind": "candidate-inf", "matches": hits})
    return record


def inventory(paths):
    files = set()
    for path in paths:
        if path.is_file():
            files.add(path)
        elif path.is_dir():
            files.update(p for p in path.rglob("*") if p.is_file() and
                         (p.suffix.lower() == ".inf" or p.name.lower() == "ccdecode.sys"))
        else:
            raise ValueError("missing input path: %s" % path)
    result, failures = [], []
    for path in sorted(files):
        try:
            record = scan_file(path)
            if record is not None:
                result.append(record)
        except (OSError, ValueError) as exc:
            failures.append({"path": str(path), "error": str(exc)})
    return {"files_examined": len(files), "candidates": result,
            "errors": failures,
            "limits": "filenames and INF text only; not CAB extraction, registration, runtime or Win98 attribution"}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=Path, help="extracted driver directories or individual files")
    args = parser.parse_args(argv)
    try:
        output = inventory(args.paths)
    except ValueError as exc:
        parser.error(str(exc))
    print(json.dumps(output, indent=2))
    return 1 if output["errors"] else 0


if __name__ == "__main__":
    sys.exit(main())
