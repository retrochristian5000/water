#!/usr/bin/env python3
"""Source-level guard for PE module personality/architecture routing.

This reads Water's actual configure.ac and aclocal.m4.  It does not claim
that a generated configure script or Windows loader has been exercised.
"""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
CONFIG = (ROOT / "configure.ac").read_text(encoding="utf-8")
MACROS = (ROOT / "aclocal.m4").read_text(encoding="utf-8")

# The left member is the configure enable_<basename> variable.  The value
# is the native PE architecture permitted by default; x86 Windows DLLs
# remain accessible to ARM64 hosts through separately built x86 emulation.
LEGACY = {
    "unicows": "dlls/unicows",
    "w95inf32": "dlls/w95inf32",
    "ntvdm": "programs/ntvdm",
}
ARCHS = ("i386", "x86_64", "arm", "aarch64", "arm64ec", "powerpc")


def fail(message):
    raise AssertionError(message)


def defaults_for(module):
    pattern = rf"^[ \t]*enable_{re.escape(module)}=\$\{{enable_{re.escape(module)}:-([^}}]+)\}}$"
    matches = re.findall(pattern, CONFIG, re.MULTILINE)
    if not matches:
        fail(f"missing configure default for {module}")
    return matches


def default_for(module):
    matches = defaults_for(module)
    if len(matches) != 1:
        fail(f"expected one configure default for {module}, found {matches}")
    return matches[0]


# Guard the owning build mechanism, not only comments or a documentation
# list.  WINE_CONFIG_MAKEFILE delegates enable_<module> to per-PE-lane
# DISABLED_SUBDIRS through wine_fn_config_makefile in aclocal.m4.
if "AS_VAR_APPEND([${i}_DISABLED_SUBDIRS]" not in MACROS:
    fail("per-architecture disabled-directory mechanism missing")
if "WINE_CONFIG_MAKEFILE" not in MACROS:
    fail("configure module-enable macro missing")

for module, module_path in LEGACY.items():
    if f"WINE_CONFIG_MAKEFILE({module_path})" not in CONFIG:
        fail(f"module {module_path} not owned by configure")
    if not (ROOT / module_path / "Makefile.in").exists():
        fail(f"missing source module {module_path}/Makefile.in")
    setting = default_for(module)
    allowed = set(setting.replace(",", " ").split())
    for arch in ARCHS:
        if (arch in allowed) != (arch == "i386"):
            fail(f"{module} incorrectly enabled for {arch}: {setting!r}")

# Controls prevent label-based exclusion: Microsoft shipped 64-bit
# mf3216.dll; WOW64 has real native ARM64 support; x64-only CPU thunk
# must not become an ARM64 PE DLL.
if "WINE_CONFIG_MAKEFILE(dlls/mf3216)" not in CONFIG:
    fail("mf3216 control module missing")
if re.search(r"^enable_mf3216=", CONFIG, re.MULTILINE):
    fail("mf3216 was incorrectly classified as i386-only")
for module, expected in (
    ("wow64", "aarch64,x86_64"),
    ("wow64win", "aarch64,x86_64"),
    ("wow64cpu", "x86_64"),
    ("xtajit64", "arm64ec"),
):
    settings = defaults_for(module)
    if settings != [expected, "no"]:
        fail(f"{module} changed PE/no-PE policy: {settings} != {[expected, 'no']}")

# The shared Win16 and VxD classes already use enable_win16=i386.
if "enable_win16=${enable_win16:-i386}" not in CONFIG:
    fail("Win16 compatibility lane is no longer i386-only")
if r"[.*\.\(.*16\|vxd\)$],[win16]" not in MACROS:
    fail("Win16 / VxD macro routing changed; re-audit before removing it")

print("PASS: native PE personality defaults reject Win9x DLL/NTVDM ARM64 ports")
print("PASS: i386 compatibility, WOW64 ARM64, x64 CPU thunk and mf3216 controls retained")
