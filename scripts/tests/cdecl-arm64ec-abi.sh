#!/bin/sh
# ABI regression for __cdecl across x86, x64, ARM64 and ARM64EC.
# Pure COFF object/archive test: not a Windows-on-Arm runtime smoke test.
set -eu
cc=${WATER_TEST_CLANG_CL:-clang-cl}
dump=${WATER_TEST_LLVM_OBJDUMP:-llvm-objdump}
linker=${WATER_TEST_LLD_LINK:-lld-link}
python=${WATER_TEST_PYTHON:-python3}
for bin in "$cc" "$dump" "$linker" "$python"; do
    command -v "$bin" >/dev/null 2>&1 || {
        echo "cdecl ABI test skipped: missing $bin" >&2
        exit 77
    }
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-cdecl-abi.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat >"$tmp/abi.c" <<'SRC'
int __cdecl whp_cdecl(int a, double b) { return a + (int)b; }
int whp_default(int a, double b) { return a + (int)b; }
int __stdcall whp_stdcall(int a, double b) { return a + (int)b; }
int __cdecl whp_vararg(int a, double b, ...) { return a + (int)b; }
__declspec(dllimport) int __cdecl whp_external(int a, double b, ...);
int whp_caller(int a, double b) { return whp_external(a, b, 3, 4.0, 5.0); }
SRC
cat >"$tmp/abi.cpp" <<'SRC'
int __cdecl whp_cpp(int a, double b) { return a + (int)b; }
extern "C" int __cdecl whp_c_linkage(int a, double b) { return a + (int)b; }
struct Whp { int __cdecl member(int a, double b); };
int Whp::member(int a, double b) { return a + (int)b; }
SRC
build_target()
{
    target=$1
    label=$2
    "$cc" "--target=$target" /nologo /GS- /c "$tmp/abi.c" "/Fo$tmp/$label.obj"
    "$cc" "--target=$target" /nologo /GS- /c "$tmp/abi.cpp" "/Fo$tmp/$label-cpp.obj"
    "$dump" --syms "$tmp/$label.obj" >"$tmp/$label.syms"
    "$dump" --syms "$tmp/$label-cpp.obj" >"$tmp/$label-cpp.syms"
}
check_symbol()
{
    if ! grep -F -- "$2" "$tmp/$1.syms" >/dev/null; then
        echo "missing $2 in $1" >&2
        cat "$tmp/$1.syms" >&2
        exit 1
    fi
}
build_target i686-pc-windows-msvc x86
build_target x86_64-pc-windows-msvc x64
build_target aarch64-pc-windows-msvc arm64
build_target arm64ec-pc-windows-msvc ec
check_symbol x86 ' _whp_cdecl'
check_symbol x86 ' _whp_default'
check_symbol x86 ' _whp_vararg'
check_symbol x86 ' _whp_stdcall@12'
check_symbol x86 ' __imp__whp_external'
check_symbol x64 ' whp_cdecl'
check_symbol x64 ' whp_default'
check_symbol x64 ' whp_vararg'
check_symbol x64 ' __imp_whp_external'
check_symbol arm64 ' whp_cdecl'
check_symbol arm64 ' whp_vararg'
check_symbol ec ' #whp_cdecl'
check_symbol ec ' #whp_vararg'
check_symbol ec ' __imp_whp_external'
check_symbol ec '$ientry_thunk$cdecl$i8$i8d'
check_symbol ec '$ientry_thunk$cdecl$i8$varargs'
check_symbol ec '$iexit_thunk$cdecl$i8$varargs'
check_symbol ec ' __os_arm64x_dispatch_ret'
check_symbol x64-cpp '?whp_cpp@@YAHHN@Z'
check_symbol ec-cpp '?whp_cpp@@$$hYAHHN@Z'
check_symbol ec-cpp ' #whp_c_linkage'
check_symbol ec-cpp '?member@Whp@@$$hQEAAHHN@Z'

# /Gv changes the default on x64, but explicit __cdecl overrides it.
for pair in x64 ec; do
    if [ "$pair" = x64 ]; then target=x86_64-pc-windows-msvc
    else target=arm64ec-pc-windows-msvc; fi
    "$cc" "--target=$target" /nologo /GS- /Gv /c "$tmp/abi.c" "/Fo$tmp/$pair-gv.obj"
    "$dump" --syms "$tmp/$pair-gv.obj" >"$tmp/$pair-gv.syms"
done
check_symbol x64-gv ' whp_default@@16'
check_symbol x64-gv ' whp_cdecl'
check_symbol ec-gv ' #whp_default'
check_symbol ec-gv ' #whp_cdecl'

cat >"$tmp/abi.def" <<'DEF'
LIBRARY whpabi.dll
EXPORTS
whp_cdecl
whp_vararg
DEF
"$linker" /lib /machine:arm64ec "/def:$tmp/abi.def" "/out:$tmp/ec.lib"
"$linker" /lib /machine:x64 "/def:$tmp/abi.def" "/out:$tmp/x64.lib"
"$python" - "$tmp/ec.lib" "$tmp/x64.lib" <<'PY'
from pathlib import Path
import struct
import sys

def scan(path):
    data = Path(path).read_bytes()
    if not data.startswith(b'!<arch>\n'):
        raise ValueError(f'{path}: not a COFF archive')
    offset = 8
    members, records = [], []
    while offset < len(data):
        header = data[offset:offset+60]
        if len(header) != 60 or header[58:60] != b'\x60\n':
            raise ValueError(f'{path}: invalid member header')
        size = int(header[48:58])
        content = data[offset+60:offset+60+size]
        if len(content) != size:
            raise ValueError(f'{path}: truncated member')
        members.append(header[:16].decode('ascii').strip())
        if len(content) >= 20 and content[:4] == b'\x00\x00\xff\xff':
            machine = struct.unpack_from('<H', content, 6)[0]
            imp_type = struct.unpack_from('<H', content, 18)[0] & 3
            names = content[20:].split(b'\0')
            records.append((machine, imp_type, names[:3]))
        offset += 60 + size + (size & 1)
    return members, records

emembers, eimports = scan(sys.argv[1])
xmembers, ximports = scan(sys.argv[2])
if '/<ECSYMBOLS>/' not in emembers or '/<ECSYMBOLS>/' in xmembers:
    raise SystemExit('missing or misplaced ARM64EC archive symbol index')
if len(eimports) != 2 or len(ximports) != 2:
    raise SystemExit(f'unexpected short imports: {eimports!r}, {ximports!r}')
for stem in (b'whp_cdecl', b'whp_vararg'):
    if (0xa641, 0, [b'#'+stem, b'whpabi.dll', stem]) not in eimports:
        raise SystemExit(f'ARM64EC import name mismatch for {stem!r}: {eimports!r}')
    if not any(machine == 0x8664 and kind == 0 and names[:2] == [stem, b'whpabi.dll']
               for machine,kind,names in ximports):
        raise SystemExit(f'x64 import name mismatch for {stem!r}: {ximports!r}')
PY
echo 'cdecl ABI: symbols, C++ $$h, varargs thunks and import libraries pass'
