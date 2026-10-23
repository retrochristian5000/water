#!/bin/sh
# Standalone, no hardware, no historical DDK, no Water/Wine build invoked.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/whp-ndis31-dma.XXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cc=${CC:-clang}
base="$root/include"
policy="$root/libs/whp-ndis31/dma_policy.c"
adapter="$root/libs/whp-ndis31/dma_ndis31.c"
flags='-std=c11 -Wall -Wextra -Werror -O1'
# GCC/Clang, rather than an invented Windows 95 compiler runtime.
"$cc" $flags -I"$base" "$policy" "$root/tests/ndis31-dma-policy.c" -o "$tmp/policy"
"$tmp/policy"
fixture="$root/tests/fixtures/ndis31-synthetic"
"$cc" $flags -D_M_IX86 -DWHP_WIN95_NDIS31_USE_HISTORICAL_DDK \
    -DWHP_WIN95_NDIS31_DMA_ADAPTER_PROBE \
    -I"$fixture" -I"$base" "$adapter" \
    "$root/tests/ndis31-dma-adapter-synthetic.c" -o "$tmp/adapter"
"$tmp/adapter"
# The artifact is a compile-only object, not linked/loaded/installed.
if "$cc" --version 2>/dev/null | grep -qi clang; then
    "$cc" --target=i386-pc-windows-msvc -std=c11 -ffreestanding \
        -fno-builtin -Wall -Wextra -Werror -I"$base" \
        -c "$policy" -o "$tmp/policy-i386.obj"
    [ -s "$tmp/policy-i386.obj" ]
    echo 'Synthetic policy i386 Windows COFF compilation passed'
fi
# Negative gate: without opt-in, no actual NDIS import should compile.
if "$cc" -std=c11 -fsyntax-only -I"$fixture" -I"$base" \
    "$adapter" >"$tmp/negative.log" 2>&1; then
    echo 'ERROR: historical DMA adapter compiled without DDK opt-in' >&2
    exit 1
fi
printf '%s\n' 'WHP Water NDIS 3.1 DMA policy and synthetic adapter smoke passed'
