#!/bin/sh
# Exercise wineld on real Clang COFF inputs and LLD-generated DLLs.
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/wineld" >&2
    exit 2
fi
wineld=$1
case "$wineld" in /*) ;; *) wineld="$(pwd)/$wineld" ;; esac
cc=${WINELD_TEST_CC:-clang}
linker=${WINELD_TEST_LINKER:-lld-link}
ar=${WINELD_TEST_AR:-llvm-ar}
for tool in "$cc" "$linker" "$ar"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "wineld test skipped: missing $tool"
        exit 77
    fi
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/wineld-test.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
printf '%s\n' '__declspec(dllexport) int wineld_probe(void) { return 42; }' > "$tmp/probe.c"
"$cc" --target=aarch64-windows -c "$tmp/probe.c" -o "$tmp/arm64.obj"
"$cc" --target=x86_64-windows -c "$tmp/probe.c" -o "$tmp/x64.obj"
"$ar" rcs "$tmp/good.lib" "$tmp/arm64.obj"
"$ar" rcs "$tmp/mixed.lib" "$tmp/arm64.obj" "$tmp/x64.obj"

expect_reject()
{
    pattern=$1
    shift
    if perl "$wineld" "$@" > "$tmp/reject.log" 2>&1; then
        echo "wineld unexpectedly accepted: $*" >&2
        exit 1
    fi
    if ! grep -F -- "$pattern" "$tmp/reject.log" >/dev/null; then
        echo "wineld rejection reason differed; expected '$pattern'" >&2
        cat "$tmp/reject.log" >&2
        exit 1
    fi
}

perl "$wineld" --target=aarch64-windows --linker="$linker" -- \
    /dll /noentry "/out:$tmp/arm64.dll" "/map:$tmp/arm64.map" /mapinfo:exports \
    "$tmp/arm64.obj" > "$tmp/result.log"
grep -F 'verified ARM64 PE32+ output' "$tmp/result.log" >/dev/null
test -s "$tmp/arm64.map" || { echo 'lld-link failed to emit /MAP output' >&2; exit 1; }
grep -F 'wineld_probe' "$tmp/arm64.map" >/dev/null || {
    echo 'link map omitted exported probe symbol' >&2; exit 1;
}
grep -F 'entry point at' "$tmp/arm64.map" >/dev/null || {
    echo 'link map omitted entry point record' >&2; exit 1;
}
perl "$wineld" --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/test.dll" "$tmp/good.lib" > "$tmp/result.log"
grep -F 'verified 1 ARM64 COFF input' "$tmp/result.log" >/dev/null

expect_reject 'expected ARM64 0xaa64' --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/bad.dll" "$tmp/x64.obj"
expect_reject 'expected ARM64 0xaa64' --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/bad.dll" "$tmp/mixed.lib"
# A DLL's implicit import-library filename must not overwrite an input .lib.
"$ar" rcs "$tmp/collision.lib" "$tmp/arm64.obj"
expect_reject 'conflicts with import-library output' --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/collision.dll" "$tmp/collision.lib"

expect_reject 'contradicts' --target=aarch64-windows --dry-run -- \
    /dll /noentry /machine:x64 "/out:$tmp/bad.dll" "$tmp/arm64.obj"
expect_reject 'unsupported target' --target=arm64ec-windows --dry-run -- \
    /dll /noentry "/out:$tmp/bad.dll" "$tmp/arm64.obj"
expect_reject 'unsupported target' --target=aarch64-w64-mingw32 --dry-run -- \
    /dll /noentry "/out:$tmp/bad.dll" "$tmp/arm64.obj"
expect_reject 'requires /dll' --target=aarch64-windows --dry-run -- \
    /noentry "/out:$tmp/bad.dll" "$tmp/arm64.obj"
expect_reject 'requires --target' --dry-run -- \
    /dll /noentry "/out:$tmp/bad.dll" "$tmp/arm64.obj"
echo 'wineld ARM64 COFF linking, archive inspection, and ABI guards: pass'
