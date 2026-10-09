#!/bin/sh
# Check the COFF machine type of every member in a Water PE libc++ archive.
# Do not accept an x64, stale, mixed-machine or non-COFF provider as AArch64.
set -eu

if [ "$#" -ne 3 ]; then
    echo 'usage: check-libcxx-archive.sh ARCHIVE TARGET LLVM_AR' >&2
    exit 2
fi
archive=$1
target=$2
ar=$3

case "$target" in
    i686-pc-windows-msvc)    expected=76:1 ;;       # IMAGE_FILE_MACHINE_I386
    x86_64-pc-windows-msvc)  expected=100:134 ;;    # IMAGE_FILE_MACHINE_AMD64
    aarch64-pc-windows-msvc) expected=100:170 ;;    # IMAGE_FILE_MACHINE_ARM64
    arm64ec-pc-windows-msvc) expected=65:166 ;;     # IMAGE_FILE_MACHINE_ARM64EC
    *) echo "unsupported libc++ PE target: $target" >&2; exit 2 ;;
esac

[ -f "$archive" ] || { echo "libc++ archive is missing: $archive" >&2; exit 1; }
members=$("$ar" t "$archive") || { echo "cannot list libc++ archive: $archive" >&2; exit 1; }
[ -n "$members" ] || { echo "empty libc++ archive: $archive" >&2; exit 1; }

# llvm-ar p NAME selects only one occurrence of duplicate member names.
# Reject ambiguous archives rather than silently skipping unchecked objects.
duplicate=$(printf '%s\n' "$members" | LC_ALL=C sort | uniq -d | sed -n '1p')
[ -z "$duplicate" ] || { echo "duplicate libc++ archive member: $duplicate" >&2; exit 1; }

printf '%s\n' "$members" | while IFS= read -r member; do
    [ -n "$member" ] || continue
    header=$("$ar" p "$archive" "$member" | od -An -tu1 -N2)
    set -- $header
    if [ "$#" -ne 2 ] || [ "$1:$2" != "$expected" ]; then
        echo "libc++ archive member '$member' has machine ${1:-?}:${2:-?}, expected $expected ($target)" >&2
        exit 1
    fi
done
