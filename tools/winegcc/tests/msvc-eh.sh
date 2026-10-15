#!/bin/sh
# Test GCC-mode winegcc's MSVC /EH exception model forwarding.
# Usage: sh tools/winegcc/tests/msvc-eh.sh /path/to/built/winegcc
set -eu
[ "$#" -eq 1 ] || { echo "usage: $0 /path/to/winegcc" >&2; exit 2; }
winegcc=$1
tmp=$(mktemp -d "${TMPDIR:-/tmp}/winegcc-eh.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
# Name this mock clang so command_uses_clang accepts it.
cat > "$tmp/clang" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" > "$WINEGCC_EH_ARGS"
exit 0
EOF
chmod +x "$tmp/clang"
printf 'int whp_c_api(void);\n' > "$tmp/input.c"

compile_eh()
{
    target=$1
    shift
    WINEGCC_EH_ARGS="$tmp/compiler-args" "$winegcc" "--target=$target" \
        --cc-cmd "$tmp/clang" -c -o "$tmp/eh.obj" "$tmp/input.c" "$@"
    # Raw MSVC switches must not be passed through as source filenames.
    if grep -E '^/EH' "$tmp/compiler-args" >/dev/null; then
        echo "MSVC /EH reached compiler as a source input" >&2
        exit 1
    fi
}
present()
{
    grep -Fx -- "$1" "$tmp/compiler-args" >/dev/null || {
        echo "missing compiler flag: $1" >&2
        cat "$tmp/compiler-args" >&2
        exit 1
    }
}
absent()
{
    if grep -Fx -- "$1" "$tmp/compiler-args" >/dev/null; then
        echo "unexpected compiler flag: $1" >&2
        cat "$tmp/compiler-args" >&2
        exit 1
    fi
}

for target in i386-pc-windows x86_64-pc-windows aarch64-pc-windows arm64ec-pc-windows; do
    compile_eh "$target" /EHs
    present -fexceptions
    absent -fexternc-nounwind
    compile_eh "$target" /EHsc
    present -fexceptions
    present -fexternc-nounwind
    present -Xclang
    compile_eh "$target" /EHs /EHc
    present -fexternc-nounwind
    compile_eh "$target" /EHc
    absent -fexceptions
    absent -fexternc-nounwind
    compile_eh "$target" /EHsc-
    present -fexceptions
    absent -fexternc-nounwind
    compile_eh "$target" /EHs /EHc-
    absent -fexternc-nounwind
    compile_eh "$target" /EHsc /EHa /EHc
    present -fasync-exceptions
    absent -fexternc-nounwind
    compile_eh "$target" /EHa /EHsc
    absent -fasync-exceptions
    present -fexternc-nounwind
done

expect_fail()
{
    expected=$1
    shift
    if WINEGCC_EH_ARGS="$tmp/compiler-args" "$winegcc" \
        --cc-cmd "$tmp/clang" -c -o "$tmp/bad.obj" \
        "$tmp/input.c" "$@" >"$tmp/error" 2>&1; then
        echo "incorrectly accepted $*" >&2
        exit 1
    fi
    grep -F -- "$expected" "$tmp/error" >/dev/null || {
        cat "$tmp/error" >&2
        exit 1
    }
}
expect_fail 'MSVC /EH requires a non-Win16 Windows PE target' \
    --target=i386-pc-windows -m16 /EHsc
expect_fail 'MSVC /EH requires a non-Win16 Windows PE target' \
    --target=i386-pc-mingw32 /EHsc
expect_fail 'unsupported MSVC /EH modifier' \
    --target=arm64ec-pc-windows /EHr
expect_fail 'missing MSVC /EH exception-model modifier' \
    --target=arm64ec-pc-windows /EH

echo 'winegcc /EHs /EHc /EHa modifier semantics and ABI guards: pass'
