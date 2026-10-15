#!/bin/sh
# Cross-compile Water's Windows SDK architecture-marker and layout contract.
# This is a header/compiler test, not a Windows loader/runtime smoke test.
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cc=${WHP_HEADER_CC:-clang}
if ! command -v "$cc" >/dev/null 2>&1; then
    echo "SKIP: header compiler unavailable: $cc" >&2
    exit 77
fi

source="$repo/scripts/tests/arm64ec-winheaders-abi.c"
[ -f "$source" ] || { echo "missing ABI fixture: $source" >&2; exit 2; }

for triple in arm64ec-pc-windows-msvc x86_64-pc-windows-msvc \
              aarch64-pc-windows-msvc i686-pc-windows-msvc
do
    echo "Windows header ABI: $triple"
    "$cc" --target="$triple" -std=c11 -fms-extensions -fsyntax-only \
        -DWIN32_LEAN_AND_MEAN -I"$repo/include" -I"$repo/include/msvcrt" \
        "$source"
done
echo 'PASS: ARM64EC SDK macros, x64 context overlay and native ABI controls'
