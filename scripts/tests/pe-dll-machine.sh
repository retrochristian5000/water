#!/bin/sh
# Standalone Clang/LLD /DLL smoke test.  This is not a Water runtime test.
set -eu

cc=${WATER_TEST_CLANG:-clang}
linker=${WATER_TEST_LLD_LINK:-lld-link}
python=${WATER_TEST_PYTHON:-python3}
for tool in "$cc" "$linker" "$python"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "PE /DLL smoke test skipped: missing $tool" >&2
        exit 77
    fi
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-pe-dll.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
printf 'int dll_data = 42;\n' > "$tmp/data.c"

check_image()
{
    triple=$1
    machine=$2
    expected_image_machine=$3
    magic=$4
    "$cc" --target="$triple" -c "$tmp/data.c" -o "$tmp/$machine.obj"
    "$linker" /dll /noentry "/machine:$machine" \
        "/map:$tmp/$machine.map" "/out:$tmp/$machine.dll" "$tmp/$machine.obj"
    test -s "$tmp/$machine.map" || {
        echo "missing /MAP output for $triple" >&2
        exit 1
    }
    "$python" - "$tmp/$machine.dll" "$expected_image_machine" "$magic" <<'PY'
import pathlib
import struct
import sys

path = pathlib.Path(sys.argv[1])
expected_machine = int(sys.argv[2], 0)
expected_magic = int(sys.argv[3], 0)
data = path.read_bytes()
if len(data) < 0x40 or data[:2] != b"MZ":
    raise SystemExit(f"{path}: no MZ stub")
pe_offset = struct.unpack_from("<I", data, 0x3c)[0]
if pe_offset + 0x40 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
    raise SystemExit(f"{path}: missing PE signature")
machine = struct.unpack_from("<H", data, pe_offset + 4)[0]
characteristics = struct.unpack_from("<H", data, pe_offset + 22)[0]
magic = struct.unpack_from("<H", data, pe_offset + 24)[0]
entry = struct.unpack_from("<I", data, pe_offset + 40)[0]
if machine != expected_machine or magic != expected_magic or not characteristics & 0x2000 or entry:
    raise SystemExit(
        f"{path}: machine={machine:#x}, magic={magic:#x}, "
        f"characteristics={characteristics:#x}, entry={entry:#x}"
    )
PY
}

check_image i686-windows-msvc x86 0x014c 0x010b
check_image aarch64-windows-msvc arm64 0xaa64 0x020b
# ARM64EC uses 0xA641 in objects, but an x64-compatible final PE image.
check_image arm64ec-windows-msvc arm64ec 0x8664 0x020b

# Object format and ABI aren't selected by /DLL alone.  Do not let an
# i386 COFF object enter a pure ARM64EC link.
if "$linker" /dll /noentry /machine:arm64ec \
    "/out:$tmp/mixed.dll" "$tmp/arm64ec.obj" "$tmp/x86.obj" \
    >"$tmp/mixed.log" 2>&1; then
    echo "LLD accepted i386 COFF input in ARM64EC /DLL link" >&2
    exit 1
fi
grep -Ei 'machine|conflict' "$tmp/mixed.log" >/dev/null || {
    echo "LLD rejection didn't identify a machine mismatch" >&2
    cat "$tmp/mixed.log" >&2
    exit 1
}

echo 'PE /DLL machine, image headers, and mixed-ABI rejection: pass'
