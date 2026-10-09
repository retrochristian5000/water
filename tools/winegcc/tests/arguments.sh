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
if WINEGCC_ARG_LOG="$tmp/args" "$winegcc" --target=i686-w64-mingw32 \
    --cc-cmd "$tmp/cc" -isystem > "$tmp/error" 2>&1; then
    echo 'missing -isystem path incorrectly accepted' >&2
    exit 1
fi
grep -F 'option -isystem requires an argument' "$tmp/error" >/dev/null ||
    { cat "$tmp/error" >&2; exit 1; }
echo 'winegcc option forwarding: pass'
