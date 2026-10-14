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


# _umul128 is a native x64 intrinsic; ARM64EC must inline it in both
# Water CRT and Clang headers rather than call an x64-only symbol.
mul_source="$repo/scripts/tests/arm64ec-mul-intrin.c"
[ -f "$mul_source" ] || { echo "missing wide-multiply test" >&2; exit 2; }
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-arm64ec-umul128.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

for provider in water llvm
do
    case "$provider" in
        water) inc="$repo/include/msvcrt" ;;
        llvm) inc="$repo/toolchains/llvm-project/clang/lib/Headers"
              if [ ! -f "$inc/intrin0.h" ]; then
                  echo "SKIP: pinned LLVM headers not checked out" >&2
                  continue
              fi ;;
    esac
    "$cc" --target=x86_64-pc-windows-msvc -ffreestanding -fms-extensions \
        -fsyntax-only -I"$inc" "$mul_source"
    "$cc" --target=arm64ec-pc-windows-msvc -ffreestanding -fms-extensions \
        -fsyntax-only -x c++ -std=c++17 -I"$inc" "$mul_source"
    "$cc" --target=arm64ec-pc-windows-msvc -ffreestanding -fms-extensions \
        -fsyntax-only -DWHP_EXISTING_MUL_ALIAS -I"$inc" "$mul_source"
    "$cc" --target=arm64ec-pc-windows-msvc -ffreestanding -fms-extensions \
        -O2 -S -I"$inc" "$mul_source" -o "$tmp/$provider.s"
    grep -Eq '[[:space:]]umulh[[:space:]]' "$tmp/$provider.s" ||
        { echo "$provider: ARM64EC high product is not lowered" >&2; exit 1; }
    if grep -Eq '^[[:space:]]*bl[[:space:]]+.*_umul128' "$tmp/$provider.s"; then
        echo "$provider: ARM64EC still calls external _umul128" >&2
        exit 1
    fi
    printf 'PASS: %s ARM64EC inline _umul128 with native UMULH\n' "$provider"
done

printf 'PASS: Water Windows ARM64, x64 and Win32 header ABI layouts\n'
