#!/bin/sh
# Self-contained COFF archive audit regressions; no Water build needed.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
checker="$root/scripts/check-libcxx-archive.sh"
clang=${TEST_CLANG:-clang}
ar=${TEST_LLVM_AR:-llvm-ar}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-libcxx-machine.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

printf 'int water_archive_test(void) { return 1; }\n' > "$tmp/probe.c"
for pair in \
    aarch64-pc-windows-msvc:aarch64 \
    arm64ec-pc-windows-msvc:arm64ec \
    x86_64-pc-windows-msvc:x86_64 \
    i686-pc-windows-msvc:i386
do
    target=${pair%%:*}
    arch=${pair#*:}
    "$clang" -target "$target" -c "$tmp/probe.c" -o "$tmp/$arch.obj"
    "$ar" rcs "$tmp/$arch.a" "$tmp/$arch.obj"
    "$checker" "$tmp/$arch.a" "$target" "$ar"
done

if "$checker" "$tmp/x86_64.a" aarch64-pc-windows-msvc "$ar" 2>/dev/null; then
    echo 'accepted wrong-architecture libc++ archive' >&2
    exit 1
fi

"$ar" rcs "$tmp/mixed.a" "$tmp/aarch64.obj" "$tmp/x86_64.obj"
if "$checker" "$tmp/mixed.a" aarch64-pc-windows-msvc "$ar" 2>/dev/null; then
    echo 'accepted mixed-architecture libc++ archive' >&2
    exit 1
fi

mkdir -p "$tmp/a" "$tmp/b"
cp "$tmp/aarch64.obj" "$tmp/a/same.obj"
cp "$tmp/x86_64.obj" "$tmp/b/same.obj"
"$ar" qc "$tmp/duplicate.a" "$tmp/a/same.obj" "$tmp/b/same.obj"
if "$checker" "$tmp/duplicate.a" aarch64-pc-windows-msvc "$ar" 2>/dev/null; then
    echo 'accepted archive with duplicate object member names' >&2
    exit 1
fi

printf 'not an archive\n' > "$tmp/junk.a"
if "$checker" "$tmp/junk.a" aarch64-pc-windows-msvc "$ar" 2>/dev/null; then
    echo 'accepted malformed archive' >&2
    exit 1
fi

printf 'PASS: Water libc++ PE archive machine guards\n'
