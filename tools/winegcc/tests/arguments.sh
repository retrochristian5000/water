#!/bin/sh
# WineGCC argument forwarding regression, using a mock compiler (no PE link).
# Usage: tools/winegcc/tests/arguments.sh /path/to/winegcc
set -eu
if [ "$#" -ne 1 ]; then echo "usage: $0 /path/to/winegcc" >&2; exit 2; fi
winegcc=$1
tmp=$(mktemp -d /tmp/winegcc-options.XXXXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/cc" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" > "$WINEGCC_ARG_LOG"
EOF
chmod +x "$tmp/cc"
echo 'int test(void) { return 0; }' > "$tmp/source.c"
mkdir "$tmp/headers"
touch "$tmp/overlay.yaml" "$tmp/header.pch"
assert_arg()
{
    if ! grep -Fx -- "$1" "$tmp/args" >/dev/null; then
        echo "winegcc dropped argument: $1" >&2
        cat "$tmp/args" >&2
        exit 1
    fi
}
run()
{
    WINEGCC_ARG_LOG="$tmp/args" "$winegcc" --target=i686-w64-mingw32 \
        --cc-cmd "$tmp/cc" -c -o "$tmp/output.o" "$@"
    assert_arg '-c'
    assert_arg "$tmp/source.c"
}
run -integrated-as "$tmp/source.c"
assert_arg '-integrated-as'
# Explicit source-language selection must not lose the operand or borrow
# the C++ runtime provider for a C-only invocation.
CXX_PE_CFLAGS='-DWHP_CXX_DRIVER=1' run -x c++ "$tmp/source.c"
assert_arg '-x'
assert_arg 'c++'
assert_arg '-DWHP_CXX_DRIVER=1'
CXX_PE_CFLAGS='-DWHP_CXX_DRIVER=1' run -xc++ "$tmp/source.c"
assert_arg '-xc++'
assert_arg '-DWHP_CXX_DRIVER=1'
CXX_PE_CFLAGS='-DWHP_CXX_DRIVER=1' run -x c "$tmp/source.c"
assert_arg 'c'
if grep -Fx -- '-DWHP_CXX_DRIVER=1' "$tmp/args" >/dev/null; then
    echo 'C object compilation unexpectedly selected C++ provider flags' >&2
    exit 1
fi
run "-isystem$tmp/headers" "$tmp/source.c"
assert_arg "-isystem$tmp/headers"
run -isystem "$tmp/headers" "$tmp/source.c"
assert_arg '-isystem'
assert_arg "$tmp/headers"
run -ivfsoverlay "$tmp/overlay.yaml" -include-pch "$tmp/header.pch" \
    -isysroot "$tmp/headers" "$tmp/source.c"
assert_arg '-ivfsoverlay'
assert_arg "$tmp/overlay.yaml"
assert_arg '-include-pch'
assert_arg "$tmp/header.pch"
assert_arg '-isysroot'
run "-isysroot$tmp/headers" "$tmp/source.c"
assert_arg "-isysroot$tmp/headers"
# On non-Apple hosts, -isysroot must remain active for Wine's fallback
# headers. The existing -isysroot forwarding tests alone cannot catch
# a missing incl_dirs declaration in the compiled non-Apple branch.
if [ "$(uname -s)" != Darwin ]; then
    if ! grep -F -- "$tmp/headers/usr/include/wine/windows" "$tmp/args" >/dev/null; then
        echo 'non-Apple -isysroot did not root fallback Wine headers' >&2
        cat "$tmp/args" >&2
        exit 1
    fi
    mkdir "$tmp/linkroot"
    run --sysroot="$tmp/linkroot" -isysroot "$tmp/headers" "$tmp/source.c"
    assert_arg "-isysroot"
    if grep -F -- "$tmp/linkroot/usr/include/wine/windows" "$tmp/args" >/dev/null; then
        echo '--sysroot wrongly overrode -isysroot for Wine headers' >&2
        exit 1
    fi
    if ! grep -F -- "$tmp/headers/usr/include/wine/windows" "$tmp/args" >/dev/null; then
        echo '-isysroot precedence over --sysroot lost for Wine headers' >&2
        exit 1
    fi
fi
# --cc-cmd is a compiler override, not a request to make every driver
# behave like Clang. Test the Windows ARM64 MSVC-ABI compiler command.
cp "$tmp/cc" "$tmp/clang"
cp "$tmp/cc" "$tmp/aarch64-windows-gcc"
cat > "$tmp/ccache" <<'EOF'
#!/bin/sh
exec "$@"
EOF
chmod +x "$tmp/clang" "$tmp/aarch64-windows-gcc" "$tmp/ccache"
run_arm64_compiler()
{
    WINEGCC_ARG_LOG="$tmp/args" "$winegcc" --target=aarch64-windows \
        --cc-cmd "$1" -c -o "$tmp/arm64.obj" "$tmp/source.c"
    assert_arg "$tmp/source.c"
}
run_arm64_compiler "$tmp/aarch64-windows-gcc"
if grep -Fx -- '-target' "$tmp/args" >/dev/null ||
   grep -Fx -- '-fuse-ld=lld' "$tmp/args" >/dev/null; then
    echo 'non-Clang ARM64 compiler received Clang-only arguments' >&2
    cat "$tmp/args" >&2
    exit 1
fi
run_arm64_compiler "$tmp/clang"
assert_arg '-target'
assert_arg 'aarch64-windows'
assert_arg '-fuse-ld=lld'
run_arm64_compiler "$tmp/ccache $tmp/clang"
assert_arg '-target'
assert_arg 'aarch64-windows'
assert_arg '-fuse-ld=lld'
if WINEGCC_ARG_LOG="$tmp/args" "$winegcc" --target=i686-w64-mingw32 \
    --cc-cmd "$tmp/cc" -isystem > "$tmp/error" 2>&1; then
    echo 'missing -isystem path incorrectly accepted' >&2
    exit 1
fi
grep -F 'option -isystem requires an argument' "$tmp/error" >/dev/null ||
    { cat "$tmp/error" >&2; exit 1; }
echo 'winegcc option forwarding: pass'
