#!/bin/sh
# Check Winemaker's generated PE target commands; no compiler/linker is run.
# Usage: sh tools/winemaker/tests/pe-target.sh /path/to/winemaker
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/winemaker" >&2
    exit 2
fi
winemaker=$1
case "$winemaker" in
    /*) ;;
    *) winemaker="$(pwd)/$winemaker" ;;
esac
tmp=$(mktemp -d "${TMPDIR:-/tmp}/winemaker-pe-target.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

make_source()
{
    mkdir "$1"
    printf '%s\n' 'int probe(void) { return 1; }' > "$1/probe.c"
    printf '%s\n' 'int cpp_probe() { return 2; }' > "$1/helper.cpp"
    printf '%s\n' '1 stdcall probe()' > "$1/probe.spec"
    printf '%s\n' 'STRINGTABLE BEGIN 1 "probe" END' > "$1/probe.rc"
}

assert_contains()
{
    if ! grep -F -- "$2" "$1" >/dev/null; then
        echo "expected '$2' in $1" >&2
        cat "$1" >&2
        exit 1
    fi
}

assert_absent()
{
    if grep -F -- "$2" "$1" >/dev/null; then
        echo "unexpected '$2' in $1" >&2
        cat "$1" >&2
        exit 1
    fi
}

make_source "$tmp/arm64"
perl "$winemaker" --nobanner --nobackup --nosource-fix --dll \
    --single-target probe --target=aarch64-windows "$tmp/arm64" > "$tmp/run.log"
makefile="$tmp/arm64/Makefile"
assert_contains "$makefile" 'WINEMAKER_TARGET = --target=aarch64-windows'
assert_contains "$makefile" 'AR = llvm-ar'
assert_contains "$makefile" 'ARFORMAT = --format=coff'
assert_contains "$makefile" '$(DLLS:%=%)'
assert_contains "$makefile" '$(probe_dll_MODULE):'
assert_contains "$makefile" '$(probe_dll_MODULE:.dll=.spec)'
assert_contains "$makefile" '-shared'
assert_contains "$makefile" '$(CC) $(WINEMAKER_TARGET) -c'
assert_contains "$makefile" '$(CXX) $(WINEMAKER_TARGET) -c'
assert_contains "$makefile" 'RC = wrc'
assert_contains "$makefile" 'RC_TARGET = $(WINEMAKER_TARGET)'
assert_contains "$makefile" 'RC_OUTPUT = -fo'
assert_contains "$makefile" '$(RC) $(RC_TARGET)'
assert_absent "$makefile" '$(DLLS:%=%.so)'
assert_absent "$makefile" '-mno-cygwin'
(
    cd "$tmp/arm64"
    make -n all > "$tmp/commands"
)
assert_contains "$tmp/commands" 'winegcc --target=aarch64-windows -c'
assert_contains "$tmp/commands" 'wineg++ --target=aarch64-windows -c'
assert_contains "$tmp/commands" 'wrc --target=aarch64-windows'
assert_contains "$tmp/commands" 'wineg++ --target=aarch64-windows -shared'
assert_contains "$tmp/commands" '-o probe.dll'

# The GNU Windows target must stay explicit too; pointer width is not an ABI.
make_source "$tmp/mingw"
perl "$winemaker" --nobanner --nobackup --nosource-fix --dll \
    --single-target probe --target aarch64-w64-mingw32 "$tmp/mingw" > "$tmp/run.log"
assert_contains "$tmp/mingw/Makefile" 'WINEMAKER_TARGET = --target=aarch64-w64-mingw32'
assert_contains "$tmp/mingw/Makefile" '$(DLLS:%=%)'

# ARM64EC must keep its distinct target and must not become plain ARM64.
make_source "$tmp/arm64ec"
perl "$winemaker" --nobanner --nobackup --nosource-fix --dll \
    --single-target probe --target=arm64ec-pc-windows "$tmp/arm64ec" > "$tmp/run.log"
assert_contains "$tmp/arm64ec/Makefile" 'WINEMAKER_TARGET = --target=arm64ec-pc-windows'
assert_contains "$tmp/arm64ec/Makefile" '$(DLLS:%=%)'

# A period Windows DLL definition file must be retained as DLL metadata.
make_source "$tmp/def"
rm "$tmp/def/probe.spec"
printf '%s\n' 'EXPORTS' 'probe' > "$tmp/def/probe.def"
perl "$winemaker" --nobanner --nobackup --nosource-fix --dll \
    --single-target probe --target=aarch64-windows "$tmp/def" > "$tmp/run.log"
assert_contains "$tmp/def/Makefile" '$(probe_dll_MODULE:.dll=.def)'
assert_contains "$tmp/def/Makefile" '-shared'
assert_absent "$tmp/def/Makefile" '$(probe_dll_MODULE:.dll=.spec)'

# Winelib (without --target) must still produce .dll.so rules.
make_source "$tmp/winelib"
perl "$winemaker" --nobanner --nobackup --nosource-fix --dll \
    --single-target probe.dll "$tmp/winelib" > "$tmp/run.log"
assert_contains "$tmp/winelib/Makefile" 'WINEMAKER_TARGET ='
assert_contains "$tmp/winelib/Makefile" 'AR = llvm-ar'
assert_contains "$tmp/winelib/Makefile" 'RC = wrc'
assert_contains "$tmp/winelib/Makefile" 'RC_OUTPUT = -fo'
if grep -F 'ARFORMAT = --format=coff' "$tmp/winelib/Makefile" >/dev/null; then
    echo 'Winelib archives unexpectedly selected COFF format' >&2
    exit 1
fi
assert_contains "$tmp/winelib/Makefile" '$(DLLS:%=%.so)'
assert_contains "$tmp/winelib/Makefile" '$(probe_dll_MODULE).so:'

# Fail closed on cross-ABI contamination and unsupported host triples.
if perl "$winemaker" --nobanner --wine32 --target=aarch64-windows \
    "$tmp/arm64" > "$tmp/error" 2>&1; then
    echo 'winemaker accepted --wine32 with an explicit Windows triple' >&2
    exit 1
fi
assert_contains "$tmp/error" '--wine32 cannot be combined with --target'

if perl "$winemaker" --nobanner --target=aarch64-apple-darwin \
    "$tmp/arm64" > "$tmp/error" 2>&1; then
    echo 'winemaker accepted a non-Windows PE target' >&2
    exit 1
fi
assert_contains "$tmp/error" 'requires a Windows PE target triple'

echo 'winemaker PE target and Winelib isolation: pass'
