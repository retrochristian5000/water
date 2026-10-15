#!/bin/sh
# Distinguish Microsoft-ABI ARM64EC C++ unwind references from GNU libunwind.
# Usage: sh scripts/tests/arm64ec-unwind-isolation.sh [configured-Water-Makefile]
set -eu
cc=${WATER_TEST_CLANGXX:-clang++}
dump=${WATER_TEST_LLVM_OBJDUMP:-llvm-objdump}
python=${WATER_TEST_PYTHON:-python3}
for bin in "$cc" "$dump" "$python"; do
    command -v "$bin" >/dev/null 2>&1 || {
        echo "ARM64EC unwind isolation test skipped: missing $bin" >&2
        exit 77
    }
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-unwind-isolation.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/eh.cpp" <<'SRC'
extern "C" int whp_api();
int whp_eh()
{
    try { if (whp_api()) throw 42; return 0; }
    catch (int e) { return e; }
}
SRC
for target in arm64ec-pc-windows-msvc aarch64-pc-windows-msvc x86_64-pc-windows-msvc; do
    "$cc" "--target=$target" -fexceptions -fcxx-exceptions -c "$tmp/eh.cpp" -o "$tmp/$target.obj"
    "$dump" --syms "$tmp/$target.obj" > "$tmp/$target.syms"
    "$dump" --section-headers "$tmp/$target.obj" > "$tmp/$target.sections"
    grep -F '__CxxFrameHandler3' "$tmp/$target.syms" >/dev/null
    grep -F '_CxxThrowException' "$tmp/$target.syms" >/dev/null
    grep -F '.pdata' "$tmp/$target.sections" >/dev/null
    grep -F '.xdata' "$tmp/$target.sections" >/dev/null
    if grep -E '(^|[[:space:]])_Unwind_[[:alnum:]_]*([[:space:]]|$)' "$tmp/$target.syms" >/dev/null; then
        echo "$target: GNU _Unwind_ runtime leaked into Microsoft C++ EH" >&2
        exit 1
    fi
done
grep -F '?whp_eh@@$$hYAHXZ' "$tmp/arm64ec-pc-windows-msvc.syms" >/dev/null
grep -F '__os_arm64x_dispatch_ret' "$tmp/arm64ec-pc-windows-msvc.syms" >/dev/null

if [ "$#" -ge 1 ]; then
    "$python" - "$1" <<'PY'
from pathlib import Path
import re
import sys

lines = Path(sys.argv[1]).read_text().splitlines()
makevars = {}
for line in lines:
    match = re.match(r'^([A-Za-z_][A-Za-z_0-9]*)\s*[:?+]?=\s*(.*)$', line)
    if match:
        makevars[match.group(1)] = match.group(2).strip()

for var in ('CXXABI_PE_LIBS', 'CXXABI_PE_CFLAGS',
            'UNWIND_PE_LIBS', 'UNWIND_PE_CFLAGS'):
    key = 'arm64ec_' + var
    if key not in makevars or makevars[key]:
        raise SystemExit(f'{key} must be explicitly empty, got {makevars.get(key)!r}')

disabled = makevars.get('arm64ec_DISABLED_SUBDIRS', '').split()
if not {'libs/c++abi', 'libs/unwind'}.issubset(disabled):
    raise SystemExit('ARM64EC legacy C++ ABI/unwind build directories still enabled')

# Match Water makedep's nested expansion with ARM64EC ABI-specific
# empty overrides taking precedence over generic GNU provider defaults.
def lookup(name):
    override = makevars.get('arm64ec_' + name)
    if override is not None and (
        override or name in ('CXXABI_PE_LIBS', 'CXXABI_PE_CFLAGS',
                             'UNWIND_PE_LIBS', 'UNWIND_PE_CFLAGS')):
        return override
    return makevars.get(name, '')

def expand(value, depth=0):
    if depth > 10:
        raise SystemExit('cyclic provider expansion')
    return re.sub(r'\$\(([A-Za-z_0-9]+)\)',
                  lambda match: expand(lookup(match.group(1)), depth+1),
                  value)

resolved = expand(lookup('CXX_PE_LIBS')).split()
if any(lib in ('c++abi', 'unwind') for lib in resolved):
    raise SystemExit(f'legacy unwind provider leaked into ARM64EC: {resolved}')
if not resolved:
    raise SystemExit('no C++ provider remains for ARM64EC')
print('ARM64EC generated Makefile: legacy GNU C++ ABI/unwind providers isolated')
PY
fi
echo 'ARM64EC Microsoft C++ EH symbols, COFF unwind sections and provider isolation: pass'
