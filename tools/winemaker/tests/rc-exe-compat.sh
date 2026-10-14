#!/bin/sh
# Compare generated wrc / LLVM llvm-rc / Microsoft rc.exe command dialects.
# Optional runtime checks run when LLVM llvm-rc and lld-link are installed.
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/winemaker" >&2
    exit 2
fi
winemaker=$1
case "$winemaker" in /*) ;; *) winemaker="$(pwd)/$winemaker" ;; esac
tmp=$(mktemp -d "${TMPDIR:-/tmp}/winemaker-rc-abi.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir "$tmp/llvm" "$tmp/microsoft" "$tmp/wrc"

for dir in llvm microsoft wrc; do
    printf '%s\n' 'int probe(void) { return 1; }' > "$tmp/$dir/probe.c"
    printf '%s\n' '1 cdecl probe()' > "$tmp/$dir/probe.spec"
    printf '%s\n' '#define MY_ID 42' 'LANGUAGE 9, 1' \
        'MY_ID RCDATA BEGIN' '  0x1234' 'END' > "$tmp/$dir/probe.rc"
done

assert_contains()
{
    if ! grep -F -- "$2" "$1" >/dev/null; then
        echo "expected $2 in $1" >&2
        cat "$1" >&2
        exit 1
    fi
}
assert_absent()
{
    if grep -F -- "$2" "$1" >/dev/null; then
        echo "unexpected $2 in $1" >&2
        cat "$1" >&2
        exit 1
    fi
}
generate()
{
    perl "$winemaker" --nobanner --nobackup --nosource-fix \
        --dll --single-target probe --target=aarch64-windows \
        "$1" "$2" > "$tmp/generation.log"
}

generate --rc=llvm-rc "$tmp/llvm"
assert_contains "$tmp/llvm/Makefile" 'RC = llvm-rc'
assert_contains "$tmp/llvm/Makefile" 'RC_TARGET ='
assert_contains "$tmp/llvm/Makefile" 'RC_OUTPUT = /FO'
assert_contains "$tmp/llvm/Makefile" '$(RC) $(RC_TARGET)'
(
    cd "$tmp/llvm"
    make -n all > "$tmp/llvm-cmds"
)
assert_contains "$tmp/llvm-cmds" 'llvm-rc'
assert_contains "$tmp/llvm-cmds" '/FOprobe.res'
assert_absent "$tmp/llvm-cmds" 'llvm-rc --target='
assert_contains "$tmp/llvm-cmds" 'winegcc --target=aarch64-windows'

generate --rc=rc.exe "$tmp/microsoft"
assert_contains "$tmp/microsoft/Makefile" 'RC = rc.exe'
assert_contains "$tmp/microsoft/Makefile" 'RC_TARGET ='
assert_contains "$tmp/microsoft/Makefile" 'RC_OUTPUT = /FO'
(
    cd "$tmp/microsoft"
    make -n all > "$tmp/ms-cmds"
)
assert_contains "$tmp/ms-cmds" 'rc.exe'
assert_contains "$tmp/ms-cmds" '/FOprobe.res'
assert_absent "$tmp/ms-cmds" 'rc.exe --target='

generate --rc=wrc "$tmp/wrc"
assert_contains "$tmp/wrc/Makefile" 'RC = wrc'
assert_contains "$tmp/wrc/Makefile" 'RC_TARGET = $(WINEMAKER_TARGET)'
assert_contains "$tmp/wrc/Makefile" 'RC_OUTPUT = -fo'
(
    cd "$tmp/wrc"
    make -n all > "$tmp/wrc-cmds"
)
assert_contains "$tmp/wrc-cmds" 'wrc --target=aarch64-windows'
assert_contains "$tmp/wrc-cmds" '-foprobe.res'

if perl "$winemaker" --nobanner --target=aarch64-windows --rc=unknown \
   "$tmp/wrc" > "$tmp/error.log" 2>&1; then
    echo 'unsupported resource compiler was accepted' >&2
    exit 1
fi
assert_contains "$tmp/error.log" 'requires wrc, llvm-rc or rc.exe'

# No architecture is encoded in an ordinary .res file. An actual LLVM RC
# output should be loadable by LLD on a Windows ARM64 target.
if command -v llvm-rc >/dev/null 2>&1 &&
   command -v lld-link >/dev/null 2>&1 &&
   command -v python3 >/dev/null 2>&1; then
    (
        cd "$tmp/llvm"
        llvm-rc /FOprobe.res probe.rc
        lld-link /machine:arm64 /dll /noentry /out:probe.dll probe.res
    )
    python3 - "$tmp/llvm/probe.res" "$tmp/llvm/probe.dll" <<'PY'
import struct
import sys
from pathlib import Path
res = Path(sys.argv[1]).read_bytes()
assert res[:8] == struct.pack("<II", 0, 32), "missing Win32 .res header"
# Parse the next resource's DWORD-aligned entry header and fields.
off = 32
size, hdr = struct.unpack_from("<II", res, off)
assert hdr >= 32 and hdr % 4 == 0
typ, typ_id, name, name_id = struct.unpack_from("<HHHH", res, off + 8)
assert (typ, typ_id, name, name_id) == (0xffff, 10, 0xffff, 42)
lang = struct.unpack_from("<H", res, off + 22)[0]
assert lang == 0x0409, hex(lang)
assert res[off + hdr:off + hdr + size] == bytes.fromhex("3412")
pe = Path(sys.argv[2]).read_bytes()
peoff = struct.unpack_from("<I", pe, 0x3c)[0]
assert pe[peoff:peoff+4] == b"PE\0\0"
assert struct.unpack_from("<H", pe, peoff+4)[0] == 0xaa64
assert struct.unpack_from("<H", pe, peoff+24)[0] == 0x020b
assert bytes.fromhex("3412") in pe, "resource not included in PE image"
PY
    echo 'LLVM llvm-rc .res -> ARM64 LLD image: pass'
else
    echo 'LLVM llvm-rc executable not installed; generator checks passed, binary comparison skipped'
fi
echo 'Winemaker resource-compiler ABI dialect isolation: pass'
