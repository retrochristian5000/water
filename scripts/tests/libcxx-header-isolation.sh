#!/bin/sh
# Prove Water's modern libc++ header staging and installed CRT isolation.
# No LLVM build, system install, or network access. Runs actual Water helpers.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
build="$root/build.sh"
installer="$root/scripts/install-libcxx.sh"
winegcc="$root/tools/winegcc/winegcc.c"
for source in "$build" "$installer" "$winegcc"; do
    [ -f "$source" ] || { echo "missing Water source: $source" >&2; exit 2; }
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-libcxx-headers.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/source/include/msvcrt" "$tmp/runtime/aarch64/include/c++/v1" \
    "$tmp/runtime/aarch64/provider" "$tmp/stage/include/wine/msvcrt"
printf '/* CRT header */\n' > "$tmp/source/include/msvcrt/corecrt.h"
printf '/* exception header */\n' > "$tmp/source/include/msvcrt/vcruntime_exception.h"
printf '#define _LIBCPP_VERSION 8000\n' > "$tmp/source/include/msvcrt/__config"
printf 'legacy C++ header\n' > "$tmp/source/include/msvcrt/string"
printf '#define _LIBCPP_VERSION 240000\n' > "$tmp/runtime/aarch64/include/c++/v1/__config_site"
printf 'modern C++ header\n' > "$tmp/runtime/aarch64/include/c++/v1/string"
printf 'COFF archive fixture\n' > "$tmp/runtime/aarch64/provider/libwhp-libcxx.a"
cp "$tmp/source/include/msvcrt/__config" "$tmp/stage/include/wine/msvcrt/__config"
cp "$tmp/source/include/msvcrt/string" "$tmp/stage/include/wine/msvcrt/string"
printf 'historical xlocinfo\n' > "$tmp/stage/include/wine/msvcrt/xlocinfo"

# Extract the unmodified production shell function, not an invented copy.
# It has a top-level closing brace and no nested top-level function.
awk '/^prepare_llvm_msvcrt_headers\(\)$/ { printing=1 }
     printing { print }
     printing && /^}$/ { exit }' "$build" > "$tmp/stage-function"
grep -Fq 'prepare_llvm_msvcrt_headers()' "$tmp/stage-function"
grep -Fq 'whp_msvcrt_unfiltered' "$tmp/stage-function"
{
    printf '%s\n' '#!/bin/sh' 'set -eu' \
        'SOURCE_DIR=$1' 'LLVM_LIBCXX_RUNTIME_DIR=$2' \
        'die() { echo "$*" >&2; exit 1; }'
    cat "$tmp/stage-function"
    printf '%s\n' 'prepare_llvm_msvcrt_headers'
} > "$tmp/run-stage"
sh -n "$tmp/run-stage"

sh "$tmp/run-stage" "$tmp/source" "$tmp/runtime" > "$tmp/overlay"
crt="$tmp/runtime/msvcrt-headers"
[ -f "$crt/corecrt.h" ] && [ -f "$crt/vcruntime_exception.h" ]
[ ! -e "$crt/__config" ] && [ ! -e "$crt/string" ]

# A matching cache signature cannot legitimize an injected libc++ 8 header.
printf 'legacy poison\n' > "$crt/__config"
sh "$tmp/run-stage" "$tmp/source" "$tmp/runtime" > "$tmp/overlay"
[ ! -e "$crt/__config" ] || { echo 'stale cache permitted libc++ 8 header' >&2; exit 1; }
# Symlinks can smuggle a legacy name through a *.h-only content filter.
ln -s corecrt.h "$crt/string"
sh "$tmp/run-stage" "$tmp/source" "$tmp/runtime" > "$tmp/overlay"
[ ! -L "$crt/string" ] || { echo 'stale cache allowed symlinked legacy header' >&2; exit 1; }
# Modifying a real CRT header invalidates and refreshes the stage as well.
printf '/* changed */\n' >> "$tmp/source/include/msvcrt/corecrt.h"
sh "$tmp/run-stage" "$tmp/source" "$tmp/runtime" > "$tmp/overlay"
grep -Fq 'changed' "$crt/corecrt.h" ||
    { echo 'cached CRT overlay missed source revision' >&2; exit 1; }

# Install the real runtime provider, including per-ABI CRT-only headers.
sh "$installer" install llvm "$tmp/runtime" 'aarch64' 1 \
    "$tmp/stage" '/include' '/lib' "$tmp/source/include/msvcrt"
[ -f "$tmp/stage/include/wine/c++/aarch64/v1/__config_site" ]
[ -f "$tmp/stage/include/wine/c++/aarch64/crt/corecrt.h" ]
[ -f "$tmp/stage/include/wine/c++/aarch64/crt/vcruntime_exception.h" ]
[ ! -e "$tmp/stage/include/wine/c++/aarch64/crt/__config" ]
[ ! -e "$tmp/stage/include/wine/c++/aarch64/crt/string" ]
# PowerPC / ARM32 / i386 may still need these historical headers.
[ -f "$tmp/stage/include/wine/msvcrt/__config" ]
[ -f "$tmp/stage/include/wine/msvcrt/string" ]

# Modern-only installation prunes old C++ headers, but never CRT-only .h
# or the historical xlocinfo exception.
sh "$installer" install llvm "$tmp/runtime" 'aarch64' 0 \
    "$tmp/stage" '/include' '/lib' "$tmp/source/include/msvcrt"
[ ! -f "$tmp/stage/include/wine/msvcrt/__config" ]
[ ! -f "$tmp/stage/include/wine/msvcrt/string" ]
[ -f "$tmp/stage/include/wine/msvcrt/xlocinfo" ]

# The postbuild probe must use the same filtered CRT as libc++'s build.
grep -Fq -- '-idirafter "$whp_libcxx_crt_headers"' "$build"
if grep -F -- '-isystem "$SOURCE_DIR/include/msvcrt"' "$build" >/dev/null; then
    echo 'libc++ postbuild probe bypasses filtered MSVCRT overlay' >&2
    exit 1
fi
# Installed C++ must not unconditionally inherit legacy wine/msvcrt.
grep -Fq 'get_installed_libcxx_crt_headers()' "$winegcc"
grep -Fq 'if (modern_crt)' "$winegcc"
grep -Fq 'reinstall Water development headers' "$winegcc"
echo 'PASS: libc++ CRT staging, cache poisoning, installation, and legacy isolation'
