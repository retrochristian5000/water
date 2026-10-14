#!/usr/bin/env python3
"""Audit a Water libc++ static COFF provider using exact target ABI symbols.

An .a/.lib archive has public defined symbols, not a DLL export directory.
Compare an ARM64EC consumer .obj to the *same target* provider archive.
Microsoft COFF manglings (including $$h names) must not be demangled or
normalized before comparison. Dependencies belonging to vcruntime/CRT/OS
and weak aliases are informational rather than falsely called libc++ gaps.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def parse_posix(output):
    symbols = {}
    for line in output.splitlines():
        parts = line.split()
        # llvm-nm -P: SYMBOL TYPE [VALUE [SIZE]]. Archive/member headings
        # have no TYPE and must not be confused with symbol definitions.
        if (len(parts) < 2 or len(parts[1]) != 1
                or parts[1] not in "aAbBcCdDiInNrRsStTuUvVwW?"):
            continue
        symbols.setdefault(parts[0], set()).add(parts[1])
    return symbols


def libcxx_name(symbol):
    # Water's private libc++ uses std::__1; unversioned std symbols may
    # instead be owned by Microsoft's vcruntime and must not be guessed.
    return "@__1@std@@" in symbol or "std::__1::" in symbol


def nm_symbols(nm, flag, path):
    cmd = [nm, "--format=posix", "--extern-only", flag, path]
    try:
        return parse_posix(subprocess.check_output(cmd, text=True, stderr=subprocess.PIPE))
    except (OSError, subprocess.CalledProcessError) as exc:
        raise RuntimeError("cannot audit symbols: " + " ".join(cmd) + ": " + str(exc)) from exc


def audit(archive_defined, archive_undefined, consumer_undefined, target,
          consumer_defined=None):
    defined = set(archive_defined)
    local_definitions = set(consumer_defined or ())

    def strong(types):
        return "U" in types or "u" in types

    required = sorted(s for s, types in consumer_undefined.items()
                      if libcxx_name(s) and strong(types))
    missing = sorted(set(required) - defined)
    weak = sorted(s for s, types in consumer_undefined.items()
                  if libcxx_name(s) and not strong(types)
                  and ("w" in types or "v" in types))
    # Clang emits many ARM64EC C++ calls as COFF weak externals. LLVM's
    # llvm-nm reports these as 'w', not 'U'. The $h variant is the
    # ARM64EC entry point: requiring the plain x64 spelling is incorrect.
    # Inline definitions in the consumer cannot be missing archive exports.
    tagged_weak = sorted(s for s in weak
                         if target.startswith("arm64ec-") and "$h" in s
                         and s not in local_definitions)
    missing_tagged = sorted(set(tagged_weak) - defined)
    # An archive has members not pulled by any particular consumer, so
    # unresolved refs within those members are evidence to inspect, not
    # automatic proof that the final link is broken.
    internal = sorted(s for s in archive_undefined
                      if libcxx_name(s) and s not in defined)
    other = sorted(s for s, types in consumer_undefined.items()
                   if not libcxx_name(s) and strong(types))
    return {
        "target": target,
        "archive_external_definitions": len(defined),
        "archive_libcxx_definitions": sum(libcxx_name(s) for s in defined),
        "provided_libcxx_symbols": sorted(s for s in defined if libcxx_name(s)),
        "required_libcxx_symbols": required,
        "missing_required_libcxx_symbols": missing,
        "required_arm64ec_weak_entry_symbols": tagged_weak,
        "missing_arm64ec_weak_entry_symbols": missing_tagged,
        "weak_libcxx_references_diagnostic": weak,
        "archive_unresolved_libcxx_references_diagnostic": internal,
        "other_provider_references_diagnostic": other,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("archive", help="libwhp-libcxx.a")
    p.add_argument("target", help="target triple of archive and probe")
    p.add_argument("probe", help="consumer object compiled for target")
    p.add_argument("--nm", default="llvm-nm")
    p.add_argument("--manifest", help="path to JSON report")
    args = p.parse_args()
    for path in (args.archive, args.probe):
        if not Path(path).is_file():
            p.error("missing ABI audit input: " + path)
    try:
        defined = nm_symbols(args.nm, "--defined-only", args.archive)
        archive_undef = nm_symbols(args.nm, "--undefined-only", args.archive)
        consumer_undef = nm_symbols(args.nm, "--undefined-only", args.probe)
        consumer_defined = nm_symbols(args.nm, "--defined-only", args.probe)
    except RuntimeError as exc:
        print("FAIL:", exc, file=sys.stderr)
        return 2
    if not defined or not consumer_undef:
        print("FAIL: empty archive definitions or consumer references", file=sys.stderr)
        return 2
    report = audit(defined, archive_undef, consumer_undef, args.target,
                   consumer_defined)
    if (not report["required_libcxx_symbols"] and
            not report["required_arm64ec_weak_entry_symbols"]):
        print("FAIL: probe has no required std::__1 references; cannot validate provider",
              file=sys.stderr)
        return 2
    if args.manifest:
        Path(args.manifest).write_text(json.dumps(report, sort_keys=True, indent=2) + "\n",
                                       encoding="utf-8")
    print("WHP libc++ %s: %d public symbols, %d std::__1 symbols, %d strong references, %d missing; %d ARM64EC weak entries, %d missing" %
          (args.target, report["archive_external_definitions"],
           report["archive_libcxx_definitions"], len(report["required_libcxx_symbols"]),
           len(report["missing_required_libcxx_symbols"]),
           len(report["required_arm64ec_weak_entry_symbols"]),
           len(report["missing_arm64ec_weak_entry_symbols"])))
    for name in report["missing_required_libcxx_symbols"]:
        print("MISSING libc++ provider symbol:", name, file=sys.stderr)
    for name in report["missing_arm64ec_weak_entry_symbols"]:
        print("MISSING libc++ ARM64EC $h entry symbol:", name, file=sys.stderr)
    print("Diagnostics: %d weak std::__1 refs, %d unresolved archive std::__1 refs, %d external-provider refs" %
          (len(report["weak_libcxx_references_diagnostic"]),
           len(report["archive_unresolved_libcxx_references_diagnostic"]),
           len(report["other_provider_references_diagnostic"])))
    return 1 if (report["missing_required_libcxx_symbols"] or
                 report["missing_arm64ec_weak_entry_symbols"]) else 0


if __name__ == "__main__":
    sys.exit(main())
