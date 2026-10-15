#!/bin/sh
# MSVC /arch in the GCC-style Water compiler wrapper must select ARM ISA
# features without becoming a source filename or crossing Win16/x86 ABIs.
# Usage: sh tools/winegcc/tests/msvc-arch.sh /path/to/built/winegcc
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/winegcc" >&2
    exit 2
fi
winegcc=$1
tmp=$(mktemp -d "${TMPDIR:-/tmp}/winegcc-msvc-arch.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/cc" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" > "$WINEGCC_ARCH_ARGS"
exit 0
EOF
chmod +x "$tmp/cc"
printf 'int whp_arch(void) { return 1; }\n' > "$tmp/input.c"

check_arch()
{
    target=$1
    value=$2
    expected=$3
    WINEGCC_ARCH_ARGS="$tmp/cc-args" "$winegcc" "--target=$target" \
        --cc-cmd "$tmp/cc" -c -o "$tmp/out.obj" \
        "$tmp/input.c" "/arch:$value"
    grep -Fx -- "-march=$expected" "$tmp/cc-args" >/dev/null || {
        echo "missing /arch translation for $target $value" >&2
        cat "$tmp/cc-args" >&2
        exit 1
    }
    if grep -F -- "/arch:$value" "$tmp/cc-args" >/dev/null; then
        echo "raw MSVC /arch escaped into compiler input list" >&2
        exit 1
    fi
}
check_arch aarch64-pc-windows armv8.0 armv8-a
check_arch arm64ec-pc-windows armv8.2+lse armv8.2-a+lse
check_arch arm64ec-pc-windows armv9.4 armv9.4-a
check_arch arm64ec-pc-windows ARMV8.3 armv8.3-a

reject_arch()
{
    expected=$1
    shift
    if WINEGCC_ARCH_ARGS="$tmp/cc-args" "$winegcc" \
        --cc-cmd "$tmp/cc" -c -o "$tmp/bad.obj" \
        "$tmp/input.c" "$@" >"$tmp/error.log" 2>&1; then
        echo "invalid MSVC /arch configuration was accepted: $*" >&2
        exit 1
    fi
    grep -F -- "$expected" "$tmp/error.log" >/dev/null || {
        cat "$tmp/error.log" >&2
        exit 1
    }
}
reject_arch 'unsupported MSVC ARM /arch value' \
    --target=arm64ec-pc-windows /arch:armv8.10
reject_arch 'unsupported MSVC ARM /arch value' \
    --target=arm64ec-pc-windows /arch:AVX2
reject_arch 'MSVC ARM /arch requires a Windows ARM64 or ARM64EC PE target' \
    --target=i386-pc-windows /arch:armv8.2
reject_arch 'MSVC ARM /arch requires a Windows ARM64 or ARM64EC PE target' \
    --target=i386-pc-windows -m16 /arch:armv8.2
reject_arch 'MSVC ARM /arch requires a Windows ARM64 or ARM64EC PE target' \
    --target=aarch64-pc-mingw32 /arch:armv8.2

echo 'Water MSVC /arch ARM64/ARM64EC translation and ABI guards: pass'
