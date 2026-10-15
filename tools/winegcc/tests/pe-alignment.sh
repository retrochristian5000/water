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
if [ -n "${WINEGCC_PE_SPEC_ARGS:-}" ]; then
    printf '%s\n' "$@" > "$WINEGCC_PE_SPEC_ARGS"
fi
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
# Keep the established alignment tests independent of the user's map policy.
WATER_PE_MAP=0
export WATER_PE_MAP

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
    case "$expected" in
        *-align:*) ;;
        *)
            if grep -F -- ',-align:' "$tmp/link-args" >/dev/null ||
               grep -F -- '-Wl,-align:' "$tmp/link-args" >/dev/null; then
                echo "redundant /ALIGN flag in default-layout PE for $target" >&2
                cat "$tmp/link-args" >&2
                exit 1
            fi
            ;;
    esac
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

# Diagnostic maps are opt-in, per-output, and MSVC PE-specific.  They
# must not change a caller-supplied /MAP path or add duplicate linker flags.
check_pe_map()
{
    target=$1
    mode=$2
    shift 2
    WATER_PE_MAP="$mode" WINEGCC_PE_LINK_ARGS="$tmp/link-args" "$winegcc" \
        "--target=$target" --cc-cmd "$tmp/cc" \
        --winebuild "$tmp/winebuild" -nodefaultlibs -nostdlib \
        -o "$tmp/output.exe" "$tmp/in.obj" "$@"
}

check_pe_map arm64ec-pc-windows 1
grep -Fx -- "-Wl,-map:$tmp/output.exe.map" "$tmp/link-args" >/dev/null
if grep -F -- '-mapinfo:exports' "$tmp/link-args" >/dev/null; then
    echo 'plain PE map unexpectedly requested exports' >&2
    exit 1
fi

check_pe_map i386-pc-windows exports
grep -Fx -- "-Wl,-map:$tmp/output.exe.map" "$tmp/link-args" >/dev/null
grep -Fx -- '-Wl,-mapinfo:exports' "$tmp/link-args" >/dev/null

check_pe_map x86_64-pc-windows 0
if grep -Ei -- '^-Wl,[-/]map(:|$)' "$tmp/link-args" >/dev/null; then
    echo 'disabled PE map policy injected /MAP' >&2
    exit 1
fi

check_pe_map aarch64-pc-windows exports "-Wl,/MAP:$tmp/explicit.map"
grep -Fx -- "-Wl,/MAP:$tmp/explicit.map" "$tmp/link-args" >/dev/null
if grep -Fx -- "-Wl,-map:$tmp/output.exe.map" "$tmp/link-args" >/dev/null; then
    echo 'PE map policy overrode explicit /MAP' >&2
    exit 1
fi

check_pe_map arm64ec-pc-windows exports -Xlinker "/MAP:$tmp/explicit-xlink.map"
if grep -Fx -- "-Wl,-map:$tmp/output.exe.map" "$tmp/link-args" >/dev/null; then
    echo 'PE map policy overrode explicit -Xlinker /MAP' >&2
    exit 1
fi
grep -Fx -- "/MAP:$tmp/explicit-xlink.map" "$tmp/link-args" >/dev/null

if check_pe_map aarch64-pc-windows unsupported >"$tmp/map-invalid.log" 2>&1; then
    echo 'invalid WATER_PE_MAP setting was accepted' >&2
    exit 1
fi
grep -F 'WATER_PE_MAP must be 0, 1, or exports' "$tmp/map-invalid.log" >/dev/null

# /DLL is a link switch, but winebuild must see DLL image semantics too.
# Raw /DLL must not make winebuild emit EXE metadata while LLD emits a DLL.
check_explicit_dll()
{
    target=$1
    shift
    WINEGCC_PE_LINK_ARGS="$tmp/link-args" WINEGCC_PE_SPEC_ARGS="$tmp/spec-args" \
        "$winegcc" "--target=$target" --cc-cmd "$tmp/cc" \
        --winebuild "$tmp/winebuild" -nodefaultlibs -nostdlib \
        -o "$tmp/explicit.dll" "$tmp/in.obj" "$@"
    grep -Fx -- '--dll' "$tmp/spec-args" >/dev/null || {
        echo "winebuild did not receive --dll for $target ($*)" >&2
        exit 1
    }
    if grep -Fx -- '--exe' "$tmp/spec-args" >/dev/null; then
        echo "winebuild received --exe for a /DLL link: $target" >&2
        exit 1
    fi
    grep -Fx -- '-shared' "$tmp/link-args" >/dev/null || {
        echo "PE /DLL did not propagate the shared-link mode: $target" >&2
        exit 1
    }
}

check_explicit_dll i386-pc-windows -Wl,/DLL
grep -Fx -- '-Wl,/DLL' "$tmp/link-args" >/dev/null
check_explicit_dll arm64ec-pc-windows -Xlinker /DLL
grep -Fx -- '/DLL' "$tmp/link-args" >/dev/null
check_explicit_dll aarch64-pc-windows -Wl,-dll
check_explicit_dll x86_64-pc-windows -shared

# A modern PE /DLL must never cause the i386 Win16 -m16 personality
# to be linked as an ARM64EC image, nor cross into the GNU link driver.
expect_dll_error()
{
    expected=$1
    shift
    if WINEGCC_PE_LINK_ARGS="$tmp/link-args" "$winegcc" \
        --cc-cmd "$tmp/cc" --winebuild "$tmp/winebuild" \
        -nodefaultlibs -nostdlib -o "$tmp/invalid.dll" "$tmp/in.obj" "$@" \
        >"$tmp/dll-error.log" 2>&1; then
        echo "invalid /DLL combination was accepted: $*" >&2
        exit 1
    fi
    grep -F -- "$expected" "$tmp/dll-error.log" >/dev/null || {
        cat "$tmp/dll-error.log" >&2
        exit 1
    }
}

expect_dll_error 'Win16 modules require a plain i386 target' \
    --target=arm64ec-pc-windows -m16 -Wl,/DLL
expect_dll_error 'MSVC /DLL requires a non-Win16 Windows PE target' \
    --target=i386-pc-windows -m16 -Wl,/DLL
expect_dll_error 'MSVC /DLL requires a non-Win16 Windows PE target' \
    --target=i386-pc-mingw32 -Wl,/DLL

# Microsoft /SUBSYSTEM must agree across winebuild metadata and the final
# MSVC linker. Comma-delimited LINK versions must not become extra -Wl flags.
check_subsystem()
{
    target=$1
    expected=$2
    shift 2
    WINEGCC_PE_LINK_ARGS="$tmp/link-args" WINEGCC_PE_SPEC_ARGS="$tmp/spec-args" \
        "$winegcc" "--target=$target" --cc-cmd "$tmp/cc" \
        --winebuild "$tmp/winebuild" -nodefaultlibs -nostdlib \
        -o "$tmp/subsystem.exe" "$tmp/in.obj" "$@"
    grep -Fx -- "$expected" "$tmp/spec-args" >/dev/null || {
        echo "winebuild subsystem mismatch: target $target, wanted $expected" >&2
        cat "$tmp/spec-args" >&2
        exit 1
    }
    link_name=${expected%%:*}
    link_version=${expected#*:}
    if [ "$link_name" = "$link_version" ]; then
        link_expected="-subsystem:$link_name"
    else
        link_expected="-subsystem:$link_name,$link_version"
    fi
    grep -Fx -- "$link_expected" "$tmp/link-args" >/dev/null || {
        echo "PE linker subsystem mismatch: target $target, wanted $link_expected" >&2
        cat "$tmp/link-args" >&2
        exit 1
    }
}

check_subsystem i386-pc-windows windows:4.0 -Wl,/SUBSYSTEM:WINDOWS,4.0
if grep -Fx -- '-Wl,4.0' "$tmp/link-args" >/dev/null ||
   grep -Fxi -- '-Wl,/SUBSYSTEM:WINDOWS' "$tmp/link-args" >/dev/null; then
    echo 'Microsoft /SUBSYSTEM was passed through with a broken comma split' >&2
    exit 1
fi
check_subsystem arm64ec-pc-windows console:10.0 -Xlinker /SUBSYSTEM:CONSOLE,10.0
check_subsystem aarch64-pc-windows efi_application:1.0 -Wl,-subsystem:EFI_APPLICATION,1.0
check_subsystem x86_64-pc-windows boot_application:1.0 \
    -Xlinker /SUBSYSTEM:BOOT_APPLICATION,1.0
check_subsystem i386-pc-windows posix:1.0 -Wl,--subsystem,posix:1.0
check_subsystem i386-pc-windows console:4.0 -mconsole -Wl,/SUBSYSTEM:CONSOLE,4.0

expect_dll_error 'Win16 modules require a plain i386 target' \
    --target=arm64ec-pc-windows -m16 -Wl,/SUBSYSTEM:WINDOWS,4.0
expect_dll_error 'MSVC /SUBSYSTEM requires a non-Win16 Windows PE target' \
    --target=i386-pc-windows -m16 -Wl,/SUBSYSTEM:WINDOWS,4.0
expect_dll_error 'MSVC /SUBSYSTEM requires a non-Win16 Windows PE target' \
    --target=i386-pc-mingw32 -Wl,/SUBSYSTEM:WINDOWS,4.0
expect_dll_error 'invalid /SUBSYSTEM version' \
    --target=i386-pc-windows -Wl,/SUBSYSTEM:WINDOWS,65536.0
expect_dll_error 'unsupported MSVC /SUBSYSTEM type' \
    --target=i386-pc-windows -Wl,/SUBSYSTEM:WINDOWSCE,5.0

echo 'winegcc PE alignment, export, ThinLTO, /MAP, /DLL, and /SUBSYSTEM arguments: pass'
