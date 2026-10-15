#!/bin/sh
# Verify Water's real cache readers and shell quoting with malformed records.
# No LLVM build, CMake configure, or Windows SDK required.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-cache-parser.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

sh -n "$root/build.sh"
for name in whp_cmake_cache_value whp_cmake_cache_optional_value whp_makefile_value shell_quote
do
    awk -v wanted="$name" '
        $0 == wanted "()" { copying=1 }
        copying { print }
        copying && /^}$/ { exit }
    ' "$root/build.sh" >> "$tmp/helpers"
done
test "$(grep -c '^whp_.*()
. "$tmp/helpers"

cat > "$tmp/CMakeCache.txt" <<'EOF'
LLVM_USE_LINKER:STRING=lld
CMAKE_GENERATOR:INTERNAL=Ninja
CMAKE_MAKE_PROGRAM:FILEPATH=/Applications/LLVM Tools/ninja
CMAKE_MSVC_RUNTIME_LIBRARY:UNINITIALIZED=
LIBCXX_EXTRA_SITE_DEFINES:STRING=_LIBCPP_NO_ABI_TAG;EQ=A
EOF
cat > "$tmp/Makefile" <<'EOF'
prefix = /Applications/Water Tools
exec_prefix = ${prefix}
libdir = ${prefix}/lib
HOST_ARCH = aarch64
PE_ARCHS = arm64ec x86_64
EOF

test "$(whp_cmake_cache_value LLVM_USE_LINKER "$tmp/CMakeCache.txt")" = lld
test "$(whp_cmake_cache_optional_value MISSING "$tmp/CMakeCache.txt")" = ''
test "$(whp_cmake_cache_value CMAKE_GENERATOR "$tmp/CMakeCache.txt")" = Ninja
test "$(whp_cmake_cache_value CMAKE_MAKE_PROGRAM "$tmp/CMakeCache.txt")" = '/Applications/LLVM Tools/ninja'
test "$(whp_cmake_cache_value CMAKE_MSVC_RUNTIME_LIBRARY "$tmp/CMakeCache.txt")" = ''
test "$(whp_cmake_cache_value LIBCXX_EXTRA_SITE_DEFINES "$tmp/CMakeCache.txt")" = '_LIBCPP_NO_ABI_TAG;EQ=A'
test "$(whp_makefile_value prefix "$tmp/Makefile")" = '/Applications/Water Tools'
test "$(whp_makefile_value exec_prefix "$tmp/Makefile")" = '${prefix}'
test "$(whp_makefile_value PE_ARCHS "$tmp/Makefile")" = 'arm64ec x86_64'

if whp_cmake_cache_value MISSING "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Missing CMake entry was silently accepted' >&2
    exit 1
fi
if whp_makefile_value MISSING "$tmp/Makefile" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Missing Makefile variable was silently accepted' >&2
    exit 1
fi

printf '%s\n' 'LIBCXX_CXX_ABI:STRING' >> "$tmp/CMakeCache.txt"
if whp_cmake_cache_value LIBCXX_CXX_ABI "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Malformed CMake entry was silently accepted' >&2
    exit 1
fi
grep -Fq 'malformed or duplicated CMake cache entry' "$tmp/err"
printf '%s\n' 'LLVM_USE_LINKER:STRING=system' >> "$tmp/CMakeCache.txt"
if whp_cmake_cache_value LLVM_USE_LINKER "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Duplicate CMake entry was silently accepted' >&2
    exit 1
fi
grep -Fq 'malformed or duplicated CMake cache entry' "$tmp/err"
if whp_cmake_cache_optional_value LLVM_USE_LINKER "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Optional CMake cache lookup silently accepted a duplicate' >&2
    exit 1
fi
grep -Fq 'malformed or duplicated CMake cache entry' "$tmp/err"

printf '%s\n' 'prefix = /other' >> "$tmp/Makefile"
if whp_makefile_value prefix "$tmp/Makefile" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Duplicate Makefile assignment was silently accepted' >&2
    exit 1
fi
grep -Fq 'duplicated Makefile assignment' "$tmp/err"

# Check the remaining sed call used for portable shell quoting.
unsafe="path's name; touch $tmp/injected"
quoted=$(shell_quote "$unsafe")
roundtrip=$(sh -c "printf '%s' $quoted")
test "$roundtrip" = "$unsafe"
test ! -e "$tmp/injected"

echo 'PASS: portable CMake/Makefile parsing, malformed input and shell quotes'
 "$tmp/helpers")" -eq 3
. "$tmp/helpers"

cat > "$tmp/CMakeCache.txt" <<'EOF'
LLVM_USE_LINKER:STRING=lld
CMAKE_GENERATOR:INTERNAL=Ninja
CMAKE_MAKE_PROGRAM:FILEPATH=/Applications/LLVM Tools/ninja
CMAKE_MSVC_RUNTIME_LIBRARY:UNINITIALIZED=
LIBCXX_EXTRA_SITE_DEFINES:STRING=_LIBCPP_NO_ABI_TAG;EQ=A
EOF
cat > "$tmp/Makefile" <<'EOF'
prefix = /Applications/Water Tools
exec_prefix = ${prefix}
libdir = ${prefix}/lib
HOST_ARCH = aarch64
PE_ARCHS = arm64ec x86_64
EOF

test "$(whp_cmake_cache_value LLVM_USE_LINKER "$tmp/CMakeCache.txt")" = lld
test "$(whp_cmake_cache_value CMAKE_GENERATOR "$tmp/CMakeCache.txt")" = Ninja
test "$(whp_cmake_cache_value CMAKE_MAKE_PROGRAM "$tmp/CMakeCache.txt")" = '/Applications/LLVM Tools/ninja'
test "$(whp_cmake_cache_value CMAKE_MSVC_RUNTIME_LIBRARY "$tmp/CMakeCache.txt")" = ''
test "$(whp_cmake_cache_value LIBCXX_EXTRA_SITE_DEFINES "$tmp/CMakeCache.txt")" = '_LIBCPP_NO_ABI_TAG;EQ=A'
test "$(whp_makefile_value prefix "$tmp/Makefile")" = '/Applications/Water Tools'
test "$(whp_makefile_value exec_prefix "$tmp/Makefile")" = '${prefix}'
test "$(whp_makefile_value PE_ARCHS "$tmp/Makefile")" = 'arm64ec x86_64'

if whp_cmake_cache_value MISSING "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Missing CMake entry was silently accepted' >&2
    exit 1
fi
if whp_makefile_value MISSING "$tmp/Makefile" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Missing Makefile variable was silently accepted' >&2
    exit 1
fi

printf '%s\n' 'LIBCXX_CXX_ABI:STRING' >> "$tmp/CMakeCache.txt"
if whp_cmake_cache_value LIBCXX_CXX_ABI "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Malformed CMake entry was silently accepted' >&2
    exit 1
fi
grep -Fq 'malformed or duplicated CMake cache entry' "$tmp/err"
printf '%s\n' 'LLVM_USE_LINKER:STRING=system' >> "$tmp/CMakeCache.txt"
if whp_cmake_cache_value LLVM_USE_LINKER "$tmp/CMakeCache.txt" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Duplicate CMake entry was silently accepted' >&2
    exit 1
fi
grep -Fq 'malformed or duplicated CMake cache entry' "$tmp/err"

printf '%s\n' 'prefix = /other' >> "$tmp/Makefile"
if whp_makefile_value prefix "$tmp/Makefile" >"$tmp/out" 2>"$tmp/err"; then
    echo 'Duplicate Makefile assignment was silently accepted' >&2
    exit 1
fi
grep -Fq 'duplicated Makefile assignment' "$tmp/err"

# Check the remaining sed call used for portable shell quoting.
unsafe="path's name; touch $tmp/injected"
quoted=$(shell_quote "$unsafe")
roundtrip=$(sh -c "printf '%s' $quoted")
test "$roundtrip" = "$unsafe"
test ! -e "$tmp/injected"

echo 'PASS: portable CMake/Makefile parsing, malformed input and shell quotes'
