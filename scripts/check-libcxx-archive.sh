#!/bin/sh
# Check the COFF machine type of every member in a Water PE libc++ archive.
# Do not accept an x64, stale, mixed-machine or non-COFF provider as AArch64.
set -eu

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo 'usage: check-libcxx-archive.sh ARCHIVE TARGET LLVM_AR [COMPONENT]' >&2
    exit 2
fi
archive=$1
target=$2
ar=$3
component=${4:-libc++}

case "$target" in
    i686-pc-windows-msvc)    expected=76:1 ;;       # IMAGE_FILE_MACHINE_I386
    x86_64-pc-windows-msvc)  expected=100:134 ;;    # IMAGE_FILE_MACHINE_AMD64
    aarch64-pc-windows-msvc) expected=100:170 ;;    # IMAGE_FILE_MACHINE_ARM64
    arm64ec-pc-windows-msvc) expected=65:166 ;;     # IMAGE_FILE_MACHINE_ARM64EC
    *) echo "unsupported $component PE target: $target" >&2; exit 2 ;;
esac

[ -f "$archive" ] || { echo "$component archive is missing: $archive" >&2; exit 1; }
members=$("$ar" t "$archive") || { echo "cannot list $component archive: $archive" >&2; exit 1; }
[ -n "$members" ] || { echo "empty $component archive: $archive" >&2; exit 1; }

# llvm-ar p NAME selects only one occurrence of duplicate member names.
# Reject ambiguous archives rather than silently skipping unchecked objects.
duplicate=$(printf '%s\n' "$members" | LC_ALL=C sort | uniq -d | sed -n '1p')
[ -z "$duplicate" ] || { echo "duplicate $component archive member: $duplicate" >&2; exit 1; }

printf '%s\n' "$members" | while IFS= read -r member; do
    [ -n "$member" ] || continue
    header=$("$ar" p "$archive" "$member" | od -An -tu1 -N8)
    set -- $header
    if [ "$#" -ge 8 ] && [ "$1:$2:$3:$4" = 0:0:255:255 ] &&
       [ "$5:$6" = 2:0 ]; then
        # COFF BigObj: Machine is at offset 6 rather than offset 0.
        machine=$7:$8
    else
        machine=${1:-?}:${2:-?}
    fi
    if [ "$machine" != "$expected" ]; then
        echo "$component archive member '$member' has machine $machine, expected $expected ($target)" >&2
        exit 1
    fi
done
