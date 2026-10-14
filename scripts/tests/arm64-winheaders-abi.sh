#!/bin/sh
# Compile Water's real Windows API headers for PE target ABIs without linking.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cc=${WHP_HEADER_CC:-clang}
command -v "$cc" >/dev/null 2>&1 || {
    printf 'SKIP: no clang compiler available: %s\n' "$cc" >&2
    exit 77
}
source="$repo/scripts/tests/arm64-winheaders-abi.c"
[ -f "$source" ] || { printf 'missing ABI test source: %s\n' "$source" >&2; exit 2; }

compile()
{
    triple=$1
    printf 'Windows header ABI: %s\n' "$triple" >&2
    "$cc" --target="$triple" -std=c11 -fms-extensions -fsyntax-only \
        -DWIN32_LEAN_AND_MEAN -I"$repo/include" -I"$repo/include/msvcrt" "$source"
}

# MSVC ARM64 is the target under investigation; x64 provides another LLP64
# control, and i686 guards the older Win32 structure contract.
compile aarch64-pc-windows-msvc
compile x86_64-pc-windows-msvc
compile i686-pc-windows-msvc

printf 'PASS: Water Windows ARM64, x64 and Win32 header ABI layouts\n'
