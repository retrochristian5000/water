#!/bin/sh
# Validate ARM64EC/ARM64X COFF import archives. Run optionally with a built
# Water winebuild executable to exercise its --implib dispatcher as well.
# Usage: sh scripts/tests/arm64ec-import-lib.sh [path/to/winebuild]
set -eu

linker=${WATER_TEST_LLD_LINK:-lld-link}
cc=${WATER_TEST_CLANG:-clang}
python=${WATER_TEST_PYTHON:-python3}
for tool in "$linker" "$cc" "$python"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ARM64EC import library test skipped: missing $tool" >&2
        exit 77
    fi
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-arm64ec-lib.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/exports.def" <<'EOF'
LIBRARY whpabi.dll
EXPORTS
    whp_func
    whp_data DATA
EOF

"$linker" /lib /machine:arm64ec "/def:$tmp/exports.def" "/out:$tmp/ec.lib"
"$linker" /lib /machine:arm64ec "/def:$tmp/exports.def" \
    "/defarm64native:$tmp/exports.def" "/out:$tmp/hybrid.lib"

printf 'int whp_extra(void) { return 7; }\n' > "$tmp/extra.c"
"$cc" --target=arm64ec-windows-msvc -c "$tmp/extra.c" -o "$tmp/extra.obj"
# The output archive can be reused as input for /lib. This preserves
# the EC-aware symbol index while extending it with another COFF member.
"$linker" /lib /machine:arm64ec "/out:$tmp/hybrid.lib" \
    "$tmp/hybrid.lib" "$tmp/extra.obj"

verify_lib()
{
    "$python" - "$1" "$2" "$3" <<'PY'
from pathlib import Path
import struct
import sys

path = Path(sys.argv[1])
native = sys.argv[2] == "native"
extra = sys.argv[3] == "extra"
data = path.read_bytes()
if not data.startswith(b"!<arch>\\n"):
    raise SystemExit(f"{path}: not a COFF archive")
offset, names, imports = 8, [], []
while offset + 60 <= len(data):
    header = data[offset:offset + 60]
    if header[58:60] != b"`\\n":
        raise SystemExit(f"{path}: invalid archive header")
    size = int(header[48:58])
    body = data[offset + 60:offset + 60 + size]
    if len(body) != size:
        raise SystemExit(f"{path}: truncated member")
    names.append(header[:16].decode("ascii").strip())
    if len(body) >= 20 and body[:4] == b"\\x00\\x00\\xff\\xff":
        machine = struct.unpack_from("<H", body, 6)[0]
        import_type = struct.unpack_from("<H", body, 18)[0] & 3
        fields = body[20:].split(b"\\0")
        if len(fields) < 2 or fields[1] != b"whpabi.dll" or not fields[0]:
            raise SystemExit(f"{path}: malformed short import record")
        imports.append((machine, import_type, fields[0]))
    offset += 60 + size + (size & 1)
if "/<ECSYMBOLS>/" not in names:
    raise SystemExit(f"{path}: missing ARM64EC symbol index")
ec = [(kind, name) for machine, kind, name in imports if machine == 0xA641]
arm = [(kind, name) for machine, kind, name in imports if machine == 0xAA64]
if len(ec) != 2 or (bool(arm) != native):
    raise SystemExit(f"{path}: wrong EC/native import views: {imports!r}")
if native and len(arm) != 2:
    raise SystemExit(f"{path}: missing native ARM64 import entries")
if (0, b"#whp_func") not in ec or (1, b"whp_data") not in ec:
    raise SystemExit(f"{path}: missing ARM64EC code/data distinction")
if extra and b"whp_extra" not in data:
    raise SystemExit(f"{path}: extra ARM64EC archive member was lost")
PY
}

verify_lib "$tmp/ec.lib" no no
verify_lib "$tmp/hybrid.lib" native extra

# A data-only import is sufficient to test IAT symbol resolution without
# relying on the separate ARM64EC __os_arm64x_dispatch_ret runtime.
cat > "$tmp/data-ref.s" <<'EOF'
.section .rdata,"dr"
.globl whp_ref
whp_ref:
.quad __imp_whp_data
EOF
"$cc" --target=arm64ec-windows-msvc -c -x assembler \
    "$tmp/data-ref.s" -o "$tmp/ref-ec.obj"
"$cc" --target=aarch64-windows-msvc -c -x assembler \
    "$tmp/data-ref.s" -o "$tmp/ref-native.obj"
"$linker" /nologo /machine:arm64ec /dll /noentry /nodefaultlib \
    "/out:$tmp/consumer-ec.dll" "$tmp/ref-ec.obj" "$tmp/hybrid.lib"
"$linker" /nologo /machine:arm64 /dll /noentry /nodefaultlib \
    "/out:$tmp/consumer-native.dll" "$tmp/ref-native.obj" "$tmp/hybrid.lib"

if [ "$#" -ge 1 ]; then
    winebuild=$1
    "$winebuild" --target=arm64ec-pc-windows --implib \
        -E "$tmp/exports.def" -o "$tmp/water-ec.lib"
    verify_lib "$tmp/water-ec.lib" no no

    "$winebuild" --target=arm64ec-pc-windows --implib \
        --without-dlltool -marm64x -E "$tmp/exports.def" \
        -o "$tmp/water-hybrid.lib" "$tmp/extra.obj"
    verify_lib "$tmp/water-hybrid.lib" native extra
fi

echo 'ARM64EC import archive machine, EC index, native view and data IAT: pass'
