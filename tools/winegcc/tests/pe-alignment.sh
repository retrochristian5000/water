#!/bin/sh
# Validate final winegcc PE link flags with mock compiler and winebuild.
# Usage: tools/winegcc/tests/pe-alignment.sh /path/to/built/winegcc
set -eu
if [ "$#" -ne 1 ]; then
    echo "usage: $0 /path/to/built/winegcc" >&2
    exit 2
fi
winegcc=$1
tmp=$(mktemp -d /tmp/winegcc-pe-align.XXXXXXXX)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/cc" <<'EOF'
#!/bin/sh
printf '%s\n' "$@" > "$WINEGCC_PE_LINK_ARGS"
exit 0
EOF
cat > "$tmp/winebuild" <<'EOF'
#!/bin/sh
out=
while [ "$#" -gt 0 ]; do
    case "$1" in
        -o) [ "$#" -ge 2 ] || exit 2; out=$2; shift 2 ;;
        *) shift ;;
    esac
done
[ -n "$out" ] || exit 2
: > "$out"
EOF
chmod +x "$tmp/cc" "$tmp/winebuild"
printf 'dummy object\n' > "$tmp/in.obj"

check_alignment()
{
    target=$1
    expected=$2
    shift 2
    WINEGCC_PE_LINK_ARGS="$tmp/link-args" "$winegcc" \
        "--target=$target" --cc-cmd "$tmp/cc" \
        --winebuild "$tmp/winebuild" -nodefaultlibs -nostdlib \
        -o "$tmp/output.exe" "$tmp/in.obj" "$@"
    if ! grep -Fx -- "$expected" "$tmp/link-args" >/dev/null; then
        echo "PE link flags incorrect for $target ($*)" >&2
        echo "expected: $expected" >&2
        cat "$tmp/link-args" >&2
        exit 1
    fi
}

# MSVC-compatible COFF linker driver, three architecture families.
check_alignment i386-pc-windows '-Wl,-filealign:0x200,-align:0x1000,-driver'
check_alignment x86_64-pc-windows '-Wl,-filealign:0x200,-align:0x1000,-driver'
check_alignment aarch64-pc-windows '-Wl,-filealign:0x200,-align:0x10000,-driver'
check_alignment arm64ec-pc-windows '-Wl,-filealign:0x200,-align:0x10000,-driver'

# File and memory alignments are separately overridable.
check_alignment i386-pc-windows '-Wl,-filealign:0x200,-align:0x2000,-driver' \
    -Wl,--section-alignment,0x2000
check_alignment i386-pc-windows '-Wl,-filealign:0x400,-align:0x1000,-driver' \
    -Wl,--file-alignment,0x400

# Special sub-page section alignments require matching on-disk layout.
check_alignment i386-pc-windows '-Wl,-filealign:0x200,-align:0x200,-driver' \
    -Wl,--section-alignment,0x200
check_alignment i386-pc-windows '-Wl,-filealign:0x400,-align:0x400,-driver' \
    -Wl,--section-alignment,0x400

# GNU driver: mock accepts the LLVM MSVC-compatible -Xlink probe.
check_alignment i386-pc-mingw32 \
    '-Wl,-Xlink=-filealign:0x200,-Xlink=-align:0x1000,-Xlink=-driver'
check_alignment i386-pc-mingw32 \
    '-Wl,-Xlink=-filealign:0x400,-Xlink=-align:0x1000,-Xlink=-driver' \
    -Wl,--file-alignment,0x400

echo 'winegcc PE alignment arguments: pass'
