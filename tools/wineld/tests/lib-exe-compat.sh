#!/bin/sh
# Compare LLVM's LIB.EXE-compatible mode with MSVC documented behavior.
# Microsoft LIB.EXE is not required in this Linux-hosted regression.
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/wineld" >&2
    exit 2
fi
wineld=$1
case "$wineld" in /*) ;; *) wineld="$(pwd)/$wineld" ;; esac
cc=${WINELD_TEST_CC:-clang}
lib=${WINELD_TEST_LIB:-lld-link}
linker=${WINELD_TEST_LINKER:-lld-link}
for tool in "$cc" "$lib" "$linker"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "wineld LIB.EXE compatibility test skipped: missing $tool"
        exit 77
    fi
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/wineld-lib-compat.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat > "$tmp/export.c" <<'SRC'
int exported(void) { return 17; }
SRC
cat > "$tmp/consumer.c" <<'SRC'
__declspec(dllimport) int exported(void);
__declspec(dllexport) int imported_call(void) { return exported(); }
SRC
"$cc" -target aarch64-windows -c "$tmp/export.c" -o "$tmp/arm64.obj"
"$cc" -target x86_64-windows -c "$tmp/export.c" -o "$tmp/x64.obj"
"$cc" -target aarch64-windows -c "$tmp/consumer.c" -o "$tmp/consumer.obj"

# Microsoft-style static archive: /machine determines the object ABI.
"$lib" /lib /machine:arm64 "/out:$tmp/static.lib" "$tmp/arm64.obj"
perl "$wineld" --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/scratch.dll" "$tmp/static.lib" > "$tmp/log"
grep -F 'verified 1 ARM64 COFF input' "$tmp/log" >/dev/null
if "$lib" /lib /machine:arm64 "/out:$tmp/mixed.lib" \
    "$tmp/arm64.obj" "$tmp/x64.obj" > "$tmp/log" 2>&1; then
    echo 'LIB.EXE-compatible mode accepted a mixed ARM64/x64 archive' >&2
    exit 1
fi
if ! grep -E 'conflicts|machine' "$tmp/log" >/dev/null; then
    echo 'LIB.EXE-compatible mode did not diagnose a machine mismatch' >&2
    cat "$tmp/log" >&2
    exit 1
fi

# An explicit DEF LIBRARY name produces a valid import archive. Then link
# a genuine ARM64 DLL importing that name, not merely an archive inspection.
printf '%s\n' 'LIBRARY period.dll' 'EXPORTS' '  exported' > "$tmp/period.def"
"$lib" /lib /machine:arm64 "/def:$tmp/period.def" "/out:$tmp/period.lib"
perl "$wineld" --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/scratch.dll" "$tmp/period.lib" > "$tmp/log"
grep -F 'verified 4 ARM64 COFF input' "$tmp/log" >/dev/null
perl "$wineld" --target=aarch64-windows --linker="$linker" -- \
    /dll /noentry "/out:$tmp/consumer.dll" "$tmp/consumer.obj" "$tmp/period.lib" > "$tmp/log"
grep -F 'verified ARM64 PE32+ output' "$tmp/log" >/dev/null
if ! grep -aF 'period.dll' "$tmp/consumer.dll" >/dev/null; then
    echo 'linked ARM64 DLL lost the declared import module name' >&2
    exit 1
fi

# Microsoft LIB.EXE infers bare.def -> bare.dll without LIBRARY.
# Older llvm-lib versions instead emit an empty import DLL name and wrong
# archive member names. Accept a corrected LLVM or require wineld to
# detect and reject the broken output; never silently accept it.
printf '%s\n' 'EXPORTS' '  exported' > "$tmp/bare.def"
"$lib" /lib /machine:arm64 "/def:$tmp/bare.def" "/out:$tmp/bare.lib"
if perl "$wineld" --target=aarch64-windows --dry-run -- \
    /dll /noentry "/out:$tmp/scratch.dll" "$tmp/bare.lib" > "$tmp/log" 2>&1; then
    if ! grep -aF 'bare.dll' "$tmp/bare.lib" >/dev/null; then
        echo 'LLVM accepted bare DEF without inferring the DLL name' >&2
        exit 1
    fi
    echo 'llvm-lib bare-DEF DLL-name fallback: implemented'
else
    if ! grep -E 'unnamed archive members|missing symbol or DLL name' "$tmp/log" >/dev/null; then
        echo 'wineld rejected bare DEF for an unexpected reason' >&2
        cat "$tmp/log" >&2
        exit 1
    fi
    echo 'llvm-lib bare-DEF DLL-name fallback: missing (guard rejected the archive)'
fi

echo 'LLVM LIB.EXE-style ARM64 archive and import ABI comparison: pass'
