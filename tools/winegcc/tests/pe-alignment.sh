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
    if grep -F -- '-driver' "$tmp/link-args" >/dev/null; then
        echo "unexpected kernel-driver flag in user-mode PE link for $target" >&2
        cat "$tmp/link-args" >&2
        exit 1
    fi
    if ! grep -Fx -- "$expected" "$tmp/link-args" >/dev/null; then
        echo "PE link flags incorrect for $target ($*)" >&2
        echo "expected: $expected" >&2
        cat "$tmp/link-args" >&2
        exit 1
    fi
}

# Standard MSVC user-mode PE layouts must not emit a redundant /ALIGN.
# LLD warns about any explicit /ALIGN without /DRIVER, even 0x1000.
check_alignment i386-pc-windows '-Wl,-filealign:0x200'
check_alignment x86_64-pc-windows '-Wl,-filealign:0x200'
check_alignment aarch64-pc-windows '-Wl,-filealign:0x200'
# Preserve the separate ARM64EC/ARM64X layout policy.
check_alignment arm64ec-pc-windows '-Wl,-filealign:0x200,-align:0x10000'
check_alignment aarch64-pc-windows '-Wl,-filealign:0x200,-align:0x10000' -marm64x

# File and memory alignments are separately overridable.
check_alignment i386-pc-windows '-Wl,-filealign:0x200,-align:0x2000' \
    -Wl,--section-alignment,0x2000
check_alignment i386-pc-windows '-Wl,-filealign:0x400' \
    -Wl,--file-alignment,0x400
check_alignment aarch64-pc-windows '-Wl,-filealign:0x200,-align:0x10000' \
    -Wl,--section-alignment,0x10000
check_alignment aarch64-pc-windows '-Wl,-filealign:0x400' \
    -Wl,--file-alignment,0x400

# Special sub-page section alignments require matching on-disk layout.
check_alignment i386-pc-windows '-Wl,-filealign:0x200,-align:0x200' \
    -Wl,--section-alignment,0x200
check_alignment i386-pc-windows '-Wl,-filealign:0x400,-align:0x400' \
    -Wl,--section-alignment,0x400

# GNU driver: mock accepts the LLVM MSVC-compatible -Xlink probe.
check_alignment i386-pc-mingw32 \
    '-Wl,-Xlink=-filealign:0x200,-Xlink=-align:0x1000'
check_alignment i386-pc-mingw32 \
    '-Wl,-Xlink=-filealign:0x400,-Xlink=-align:0x1000' \
    -Wl,--file-alignment,0x400

# ThinLTO should only keep referenced PE sections, including required
# winecrt0 startup code.  Do not impose GC for non-LTO or full-LTO links.
check_thin_gc()
{
    target=$1
    gc_flag=$2
    expected=$3
    shift 3
    WINEGCC_PE_LINK_ARGS="$tmp/link-args" "$winegcc" \
        "--target=$target" --cc-cmd "$tmp/cc" \
        --winebuild "$tmp/winebuild" -nodefaultlibs -nostdlib \
        -shared -o "$tmp/output.dll" "$tmp/in.obj" "$@"
    count=$(grep -Fxc -- "$gc_flag" "$tmp/link-args" || true)
    if [ "$count" -ne "$expected" ]; then
        echo "incorrect ThinLTO section GC for $target ($*); expected $expected, got $count" >&2
        cat "$tmp/link-args" >&2
        exit 1
    fi
}

check_thin_gc i386-pc-mingw32 '-Wl,--gc-sections' 1 -flto=thin
check_thin_gc i386-pc-mingw32 '-Wl,--gc-sections' 0 -flto=full
check_thin_gc i386-pc-mingw32 '-Wl,--gc-sections' 0 -flto=thin -fno-lto
check_thin_gc i386-pc-mingw32 '-Wl,--gc-sections' 0 -flto=thin -Wl,--no-gc-sections
check_thin_gc x86_64-pc-windows '-Wl,-opt:ref' 1 -flto=thin
check_thin_gc x86_64-pc-windows '-Wl,-opt:ref' 0
check_thin_gc x86_64-pc-windows '-Wl,-opt:ref' 0 -flto=thin -Wl,-opt:noref

# stdcall @NN export decoration exists on i386, not on ARM64/ARM64EC.
# Never strip @ from legitimate non-i386 user-mode DLL export names.
check_kill_at()
{
    target=$1
    expected=$2
    case "$target" in
        *mingw*) flag='-Wl,--kill-at' ;;
        *) flag='-Wl,-kill-at' ;;
    esac
    WINEGCC_PE_LINK_ARGS="$tmp/link-args" "$winegcc" \
        "--target=$target" --cc-cmd "$tmp/cc" \
        --winebuild "$tmp/winebuild" -nodefaultlibs -nostdlib \
        -shared -o "$tmp/output.dll" "$tmp/in.obj"
    count=$(grep -Fxc -- "$flag" "$tmp/link-args" || true)
    if [ "$count" -ne "$expected" ]; then
        echo "incorrect DLL export-decoration policy for $target: expected $expected, got $count" >&2
        cat "$tmp/link-args" >&2
        exit 1
    fi
}
check_kill_at i386-pc-windows 1
check_kill_at x86_64-pc-windows 0
check_kill_at aarch64-pc-windows 0
check_kill_at arm64ec-pc-windows 0
check_kill_at i386-pc-mingw32 1
check_kill_at aarch64-pc-mingw32 0

echo 'winegcc PE alignment, DLL exports, and ThinLTO GC arguments: pass'
