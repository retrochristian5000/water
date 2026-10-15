#!/bin/sh
# Microsoft's conform pragma affects C++ for-loop scope, not the COFF ABI.
# Optional freshly built winegcc validates the /Zc:forScope bridge.
set -eu
cc=${WATER_TEST_CLANG_CL:-clang-cl}
dump=${WATER_TEST_LLVM_OBJDUMP:-llvm-objdump}
for bin in "$cc" "$dump"; do
    command -v "$bin" >/dev/null 2>&1 || { echo "SKIP: missing $bin" >&2; exit 77; }
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-conform.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat >"$tmp/pragma.cpp" <<'SRC'
#pragma conform(forScope, off)
extern "C" int whp_conform(int x) { for (int i = 0; i != 1; ++i) {} return x; }
SRC
cat >"$tmp/plain.cpp" <<'SRC'
extern "C" int whp_conform(int x) { for (int i = 0; i != 1; ++i) {} return x; }
SRC
cat >"$tmp/legacy.cpp" <<'SRC'
#pragma conform(forScope, off)
int whp_out_of_scope() { for (int i = 0; i != 1; ++i) {} return i; }
SRC
cat >"$tmp/show.cpp" <<'SRC'
#pragma conform(forScope, show)
int whp_show() { return 0; }
SRC
for target in i686-pc-windows-msvc x86_64-pc-windows-msvc \
              aarch64-pc-windows-msvc arm64ec-pc-windows-msvc
do
    if "$cc" --target="$target" /nologo /c /clang:-fsyntax-only \
        /clang:-Werror=unknown-pragmas "$tmp/pragma.cpp" >"$tmp/unknown.log" 2>&1; then
        echo "$target: legacy pragma unexpectedly supported" >&2; exit 1
    fi
    grep -F 'unknown pragma ignored' "$tmp/unknown.log" >/dev/null
    if "$cc" --target="$target" /nologo /c /clang:-fsyntax-only \
        /clang:-Werror=unknown-pragmas "$tmp/show.cpp" >"$tmp/show.log" 2>&1; then
        echo "$target: show pragma unexpectedly supported" >&2; exit 1
    fi
    grep -F 'unknown pragma ignored' "$tmp/show.log" >/dev/null
    if "$cc" --target="$target" /nologo /c /Zc:forScope- \
        /clang:-Wno-unknown-pragmas /clang:-fsyntax-only \
        "$tmp/legacy.cpp" >"$tmp/legacy.log" 2>&1; then
        echo "$target: legacy loop-scope variable unexpectedly accepted" >&2; exit 1
    fi
    grep -F "use of undeclared identifier 'i'" "$tmp/legacy.log" >/dev/null
    "$cc" --target="$target" /nologo /c /W0 "$tmp/pragma.cpp" "/Fo$tmp/pragma.obj"
    "$cc" --target="$target" /nologo /c /W0 "$tmp/plain.cpp" "/Fo$tmp/plain.obj"
    "$dump" --syms "$tmp/pragma.obj" >"$tmp/pragma.syms"
    "$dump" --syms "$tmp/plain.obj" >"$tmp/plain.syms"
    case "$target" in
        i686*) symbol=_whp_conform ;;
        arm64ec*) symbol='#whp_conform' ;;
        *) symbol=whp_conform ;;
    esac
    grep -F -- " $symbol" "$tmp/pragma.syms" >/dev/null
    grep -F -- " $symbol" "$tmp/plain.syms" >/dev/null
    echo "PASS: $target scope and COFF C-name controls"
done
if [ "$#" -ge 1 ]; then
    winegcc=$1
    cat >"$tmp/clang" <<'MOCK'
#!/bin/sh
printf '%s\n' "$@" >"$WHP_CONFORM_ARGS"
exit 0
MOCK
    chmod +x "$tmp/clang"
    for target in i386-pc-windows x86_64-pc-windows aarch64-pc-windows arm64ec-pc-windows; do
        WHP_CONFORM_ARGS="$tmp/args" "$winegcc" --target="$target" \
            --cc-cmd "$tmp/clang" -c -o "$tmp/out.obj" "$tmp/plain.cpp" /Zc:forScope
        if grep -F '/Zc:forScope' "$tmp/args" >/dev/null; then
            echo "MSVC /Zc:forScope leaked to GCC-style compiler" >&2; exit 1
        fi
        if WHP_CONFORM_ARGS="$tmp/args" "$winegcc" --target="$target" \
            --cc-cmd "$tmp/clang" -c -o "$tmp/no.obj" "$tmp/plain.cpp" \
            /Zc:forScope- >"$tmp/error" 2>&1; then
            echo "$target: winegcc accepted legacy scope" >&2; exit 1
        fi
        grep -F 'MSVC /Zc:forScope- is unsupported' "$tmp/error" >/dev/null
    done
    if WHP_CONFORM_ARGS="$tmp/args" "$winegcc" --target=i386-pc-windows -m16 \
        --cc-cmd "$tmp/clang" -c -o "$tmp/win16.obj" \
        "$tmp/plain.cpp" /Zc:forScope >"$tmp/win16-error" 2>&1; then
        echo 'Win16 improperly accepted C++ scope switch' >&2; exit 1
    fi
    grep -F 'MSVC /Zc:forScope requires a non-Win16 Windows PE target' \
        "$tmp/win16-error" >/dev/null
fi
echo 'PASS: conform pragma is not an ARM64EC ABI switch'
