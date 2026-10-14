#!/bin/sh
# Compile actual Water COM headers for ordinary ARM64/x64/i386 Windows.
# Needs the generated WIDL headers in a configured Water build directory.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
cc=${WHP_COM_CC:-clang}
cxx=${WHP_COM_CXX:-clang++}
for tool in "$cc" "$cxx"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        printf 'SKIP: compiler %s unavailable\n' "$tool" >&2
        exit 77
    fi
done

# A source checkout contains oaidl.idl and unknwn.idl, not the WIDL-
# generated oaidl.h/unknwn.h. Require a real generated-header path rather
# than silently using another toolchain's similarly named SDK headers.
generated=${WHP_COM_GENERATED_INCLUDE:-"${WHP_BUILD_DIR:-$(pwd)}/include"}
if [ ! -f "$generated/oaidl.h" ] || [ ! -f "$generated/unknwn.h" ] ||
   [ ! -f "$generated/objidlbase.h" ]; then
    printf '%s\n' 'SKIP: generated COM headers not found in:' >&2
    printf '  %s\n' "$generated" >&2
    printf '%s\n' 'Configure/build Water first, then set WHP_COM_GENERATED_INCLUDE to its build/include directory.' >&2
    exit 77
fi

fixture="$root/scripts/tests/com-abi-headers.c"
for triple in aarch64-pc-windows-msvc x86_64-pc-windows-msvc i686-pc-windows-msvc; do
    printf 'COM C ABI: %s\n' "$triple" >&2
    "$cc" --target="$triple" -std=c11 -fms-extensions -fsyntax-only \
        -I"$generated" -I"$root/include" -I"$root/include/msvcrt" -x c "$fixture"
    printf 'COM C++ ABI: %s\n' "$triple" >&2
    "$cxx" --target="$triple" -std=c++17 -fms-extensions -fsyntax-only \
        -I"$generated" -I"$root/include" -I"$root/include/msvcrt" -x c++ "$fixture"
    printf 'COM CINTERFACE ABI: %s\n' "$triple" >&2
    "$cxx" --target="$triple" -std=c++17 -fms-extensions -DCINTERFACE -fsyntax-only \
        -I"$generated" -I"$root/include" -I"$root/include/msvcrt" -x c++ "$fixture"
done
printf '%s\n' 'PASS: Water COM root IID, IUnknown/IDispatch, VARIANT and SAFEARRAY contracts'
