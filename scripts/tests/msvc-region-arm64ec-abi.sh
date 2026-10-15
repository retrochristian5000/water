#!/bin/sh
# Microsoft region/endregion is editor outlining: no ABI, COFF or thunk effect.
# Compare COFF objects from one C++ source with region markers toggled.
set -eu
cc=${WATER_TEST_CLANG_CL:-clang-cl}
dump=${WATER_TEST_LLVM_OBJDUMP:-llvm-objdump}
python=${WATER_TEST_PYTHON:-python3}
for bin in "$cc" "$dump" "$python"; do
    command -v "$bin" >/dev/null 2>&1 || {
        echo "SKIP: required test tool unavailable: $bin" >&2
        exit 77
    }
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-region.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/region.cpp" <<'CPP'
#include <stddef.h>
#if defined(WHP_REGION)
#pragma region Public_ARM64EC_ABI
#pragma region Nested_Layout
#endif
struct whp_record { double amount; int count; void *pointer; };
#if defined(_WIN64)
static_assert(sizeof(whp_record) == 24, "64-bit record size");
static_assert(offsetof(whp_record, pointer) == 16, "64-bit pointer offset");
#else
static_assert(sizeof(whp_record) == 16, "32-bit record size");
static_assert(offsetof(whp_record, pointer) == 12, "32-bit pointer offset");
#endif
#if defined(WHP_REGION)
#pragma endregion Nested_Layout
#endif
extern "C" __declspec(dllexport) int __cdecl whp_region_export(whp_record r, int x)
{
#if defined(WHP_REGION)
    __pragma(region Inline_Body)
#endif
    int result = int(r.amount) + r.count + x + (r.pointer != nullptr);
#if defined(WHP_REGION)
    __pragma(endregion Inline_Body)
#endif
    return result;
}
#if defined(WHP_REGION)
#pragma endregion Public_ARM64EC_ABI
#endif
CPP
for target in i686-pc-windows-msvc x86_64-pc-windows-msvc \
              aarch64-pc-windows-msvc arm64ec-pc-windows-msvc
do
    "$cc" --target="$target" /nologo /GS- /c \
        /clang:-Werror=unknown-pragmas "$tmp/region.cpp" "/Fo$tmp/plain.obj"
    "$cc" --target="$target" /nologo /GS- /c /DWHP_REGION \
        /clang:-Werror=unknown-pragmas "$tmp/region.cpp" "/Fo$tmp/region.obj"
    # COFF bytes 4..7 contain TimeDateStamp. They can change when the two
    # commands cross a one-second boundary; compare all other bytes exactly.
    "$python" - "$tmp/plain.obj" "$tmp/region.obj" "$target" <<'PYTEST'
from pathlib import Path
import sys

plain = bytearray(Path(sys.argv[1]).read_bytes())
region = bytearray(Path(sys.argv[2]).read_bytes())
machine = {'i686-pc-windows-msvc': 0x14c,
           'x86_64-pc-windows-msvc': 0x8664,
           'aarch64-pc-windows-msvc': 0xaa64,
           'arm64ec-pc-windows-msvc': 0xa641}[sys.argv[3]]
if len(plain) < 20 or len(region) < 20 or int.from_bytes(plain[:2], 'little') != machine or int.from_bytes(region[:2], 'little') != machine:
    raise SystemExit(f'{sys.argv[3]}: missing or incorrect COFF machine header')
plain[4:8] = region[4:8] = b'\0' * 4
if plain != region:
    raise SystemExit(f'{sys.argv[3]}: region changed more than COFF TimeDateStamp')
PYTEST
    "$dump" --syms "$tmp/region.obj" > "$tmp/syms"
    case "$target" in
        i686*) symbol=_whp_region_export ;;
        arm64ec*) symbol='#whp_region_export' ;;
        *) symbol=whp_region_export ;;
    esac
    grep -F -- " $symbol" "$tmp/syms" >/dev/null || {
        echo "$target: C linkage symbol missing: $symbol" >&2; exit 1;
    }
    if [ "$target" = arm64ec-pc-windows-msvc ]; then
        grep -F '$ientry_thunk$cdecl$' "$tmp/syms" >/dev/null || {
            echo 'ARM64EC entry thunk absent' >&2; exit 1;
        }
        grep -F '__os_arm64x_dispatch_ret' "$tmp/syms" >/dev/null || {
            echo 'ARM64EC return dispatch dependency absent' >&2; exit 1;
        }
    fi
    echo "PASS: $target region/endregion preserved COFF content (timestamp normalized)"
done
