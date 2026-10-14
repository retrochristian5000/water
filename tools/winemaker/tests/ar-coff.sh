#!/bin/sh
# Test Winemaker's llvm-ar command selection and native ARM64 COFF members.
# Usage: sh tools/winemaker/tests/ar-coff.sh /path/to/winemaker
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/winemaker" >&2
    exit 2
fi
winemaker=$1
case "$winemaker" in /*) ;; *) winemaker="$(pwd)/$winemaker" ;; esac
for tool in perl make clang llvm-ar llvm-objdump; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "winemaker llvm-ar test skipped: missing $tool"
        exit 77
    fi
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/winemaker-ar-coff.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

check()
{
    if ! grep -F -- "$2" "$1" >/dev/null; then
        echo "expected '$2' in $1" >&2
        cat "$1" >&2
        exit 1
    fi
}

source_tree()
{
    mkdir "$1"
    printf '%s\n' 'int probe(void) { return 7; }' > "$1/probe.c"
}

source_tree "$tmp/arm64"
perl "$winemaker" --nobanner --nobackup --nosource-fix \
    --lib --single-target probe --target=aarch64-windows "$tmp/arm64" > "$tmp/generate.log"
makefile="$tmp/arm64/Makefile"
check "$makefile" 'AR = llvm-ar'
check "$makefile" 'ARFORMAT = --format=coff'
check "$makefile" '$(AR) $(ARFORMAT) $(probe_a_ARFLAGS)'
check "$makefile" 'probe_a_ARFLAGS'
check "$makefile" 'rcs'
(
    cd "$tmp/arm64"
    make -n all > "$tmp/dry-run.log"
)
check "$tmp/dry-run.log" 'llvm-ar --format=coff'
check "$tmp/dry-run.log" 'rcs probe.a'
(
    cd "$tmp/arm64"
    make CC=clang >/dev/null
)
test -f "$tmp/arm64/probe.a"
llvm-ar t "$tmp/arm64/probe.a" > "$tmp/members.log"
check "$tmp/members.log" 'probe.o'
llvm-objdump -f "$tmp/arm64/probe.a" > "$tmp/architecture.log"
check "$tmp/architecture.log" 'file format coff-arm64'
check "$tmp/architecture.log" 'architecture: aarch64'

# Ensure that LLD really consumes the generated COFF library and resolves
# the provider symbol. Reading an archive's members alone is not sufficient.
if command -v lld-link >/dev/null 2>&1; then
    printf '%s\n' \
        'extern int probe(void);' \
        '__declspec(dllexport) int use_probe(void) { return probe(); }' \
        > "$tmp/consumer.c"
    clang --target=aarch64-windows -c "$tmp/consumer.c" -o "$tmp/consumer.obj"
    lld-link /dll /noentry "/out:$tmp/consumer.dll" \
        "$tmp/consumer.obj" "$tmp/arm64/probe.a"
    llvm-objdump -f "$tmp/consumer.dll" > "$tmp/linked-image.log"
    check "$tmp/linked-image.log" 'file format coff-arm64'
fi

# Do not force PE/COFF archive format on Winelib or Unix host outputs.
source_tree "$tmp/winelib"
perl "$winemaker" --nobanner --nobackup --nosource-fix \
    --lib --single-target probe.a "$tmp/winelib" > "$tmp/generate.log"
check "$tmp/winelib/Makefile" 'AR = llvm-ar'
if grep -F 'ARFORMAT = --format=coff' "$tmp/winelib/Makefile" >/dev/null; then
    echo "Winelib archiver was forced into PE/COFF mode" >&2
    exit 1
fi
(
    cd "$tmp/winelib"
    make -n all > "$tmp/dry-run.log"
)
check "$tmp/dry-run.log" 'llvm-ar'
check "$tmp/dry-run.log" 'rcs probe.a'

# GNU Windows is also COFF, not the host's ELF or Mach-O archive mode.
source_tree "$tmp/mingw"
perl "$winemaker" --nobanner --nobackup --nosource-fix \
    --lib --single-target probe --target=aarch64-w64-mingw32 "$tmp/mingw" > "$tmp/generate.log"
check "$tmp/mingw/Makefile" 'AR = llvm-ar'
check "$tmp/mingw/Makefile" 'ARFORMAT = --format=coff'

echo 'winemaker llvm-ar selection, ARM64 COFF member and Winelib isolation: pass'
