#!/bin/sh
# Validate COFF LIB /MACHINE selection and the ARM64EC hybrid archive index.
# Optional argument: a built Water winebuild executable for target-boundary tests.
set -eu
cc=${WATER_TEST_CLANG:-clang}
lib=${WATER_TEST_LLD_LINK:-lld-link}
python=${WATER_TEST_PYTHON:-python3}
for tool in "$cc" "$lib" "$python"; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "LIB /MACHINE test skipped: missing $tool" >&2
        exit 77
    }
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-lib-machine.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
make_obj()
{
    printf 'int whp_%s(void) { return 7; }\n' "$2" > "$tmp/$2.c"
    "$cc" --target="$1" -c "$tmp/$2.c" -o "$tmp/$2.obj"
}
make_obj i686-windows-msvc x86
make_obj x86_64-windows-msvc x64
make_obj aarch64-windows-msvc arm64
make_obj arm64ec-windows-msvc arm64ec
reject()
{
    if "$lib" /lib "/machine:$1" "/out:$tmp/reject.lib" "$tmp/$2.obj" >"$tmp/reject.log" 2>&1; then
        echo "/machine:$1 incorrectly accepted $2" >&2
        exit 1
    fi
    grep -Ei 'machine type.*conflict' "$tmp/reject.log" >/dev/null || {
        cat "$tmp/reject.log" >&2; exit 1;
    }
}
reject arm64ec x86
reject arm64 arm64ec
reject arm64 x64
reject x64 arm64ec
reject x64 arm64
reject arm64 x86
if "$lib" /lib "/out:$tmp/undetected.lib" "$tmp/arm64ec.obj" >"$tmp/err.log" 2>&1; then
    echo 'ARM64EC without explicit /machine was accepted' >&2
    exit 1
fi
grep -Ei 'machine type.*conflict|use /machine:arm64ec' "$tmp/err.log" >/dev/null

# ARM64EC and ARM64X accept separate x64-compatible and native ARM64 views.
"$lib" /lib /machine:arm64ec "/out:$tmp/ec.lib" "$tmp/arm64ec.obj" "$tmp/x64.obj" "$tmp/arm64.obj"
"$lib" /lib /machine:arm64x "/out:$tmp/x.lib" "$tmp/arm64ec.obj" "$tmp/x64.obj" "$tmp/arm64.obj"
"$lib" /lib /machine:arm64 "/out:$tmp/native.lib" "$tmp/arm64.obj"
"$lib" /lib /machine:x86 "/out:$tmp/x86.lib" "$tmp/x86.obj"
"$python" - "$tmp" <<'PY'
from pathlib import Path
import struct
import sys

root = Path(sys.argv[1])
expected = {
    "ec.lib": ({0xa641, 0x8664, 0xaa64}, True),
    "x.lib": ({0xa641, 0x8664, 0xaa64}, True),
    "native.lib": ({0xaa64}, False),
    "x86.lib": ({0x014c}, False),
}
for filename, (machines, ec_index) in expected.items():
    data = (root / filename).read_bytes()
    if not data.startswith(b"!<arch>\n"):
        raise SystemExit(f"{filename}: invalid archive magic")
    offset, found, names = 8, set(), []
    while offset + 60 <= len(data):
        header = data[offset:offset + 60]
        if header[58:60] != bytes((0x60, 10)):
            raise SystemExit(f"{filename}: invalid archive header")
        size = int(header[48:58])
        name = header[:16].decode("ascii").strip()
        body = data[offset + 60:offset + 60 + size]
        if len(body) != size:
            raise SystemExit(f"{filename}: truncated member")
        names.append(name)
        if name.startswith("/") and name not in ("/", "//", "/<ECSYMBOLS>/"):
            if len(body) >= 20:
                found.add(struct.unpack_from("<H", body, 0)[0])
        offset += 60 + size + (size & 1)
    if found != machines or ("/<ECSYMBOLS>/" in names) != ec_index:
        raise SystemExit(f"{filename}: machines={found}, indexes={names!r}")
PY

if [ "$#" -ge 1 ]; then
    winebuild=$1
    if "$winebuild" --target=arm64ec-pc-windows -m16 --staticlib \
        -o "$tmp/bad.lib" "$tmp/x64.obj" >"$tmp/win16.log" 2>&1; then
        echo 'Win16 static archive wrongly accepted for ARM64EC' >&2
        exit 1
    fi
    grep -F 'Win16 modules require an i386 target' "$tmp/win16.log" >/dev/null || {
        cat "$tmp/win16.log" >&2; exit 1;
    }
    if "$winebuild" --target=i386-pc-windows -marm64x --implib \
        -o "$tmp/bad.lib" -E "$tmp/missing.def" >"$tmp/bad-x.log" 2>&1; then
        echo 'ARM64X import library wrongly accepted for i386' >&2
        exit 1
    fi
    grep -F -- '-marm64x import libraries require an ARM64EC target' "$tmp/bad-x.log" >/dev/null || {
        cat "$tmp/bad-x.log" >&2; exit 1;
    }
fi
echo 'LIB /MACHINE architecture matrix, EC symbol index, Win16 target guard: pass'
