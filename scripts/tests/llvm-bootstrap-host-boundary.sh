#!/bin/sh
# Test Water's actual bootstrap helpers without rebuilding LLVM.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-llvm-host.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
mkdir -p "$tmp/bin" "$tmp/build"
sh -n "$root/build.sh"

cat > "$tmp/bin/uname" <<'EOF'
#!/bin/sh
case "$1" in
    -s) echo Darwin ;;
    -m) echo "${MOCK_HOST_ARCH:-arm64}" ;;
    *) exit 2 ;;
esac
EOF
cat > "$tmp/bin/cc" <<'EOF'
#!/bin/sh
if [ "$1" = -print-target-triple ]; then
    echo "${MOCK_CC_TRIPLE:-arm64-apple-darwin23.0.0}"
    exit 0
fi
cat >/dev/null
exit "${MOCK_CC_FAIL:-0}"
EOF
cat > "$tmp/bin/cxx" <<'EOF'
#!/bin/sh
if [ "$1" = -print-target-triple ]; then
    echo "${MOCK_CXX_TRIPLE:-arm64-apple-darwin23.0.0}"
    exit 0
fi
cat >/dev/null
exit "${MOCK_CXX_FAIL:-0}"
EOF
chmod +x "$tmp/bin/"*
PATH="$tmp/bin:$PATH"
export PATH
{
    printf '%s\n' '#!/bin/sh' 'set -eu'
    printf "WHP_TEST_DIR='%s'\nLLVM_BOOTSTRAP_DIR='%s/build'\n" "$tmp" "$tmp"
    cat <<'EOF'
LLVM_BOOTSTRAP_RECIPE=9
WATER_LLVM_BUILD_TYPE=Release
WATER_LLVM_ASSERTIONS=n
WATER_LLVM_LEAN=y
WATER_LLVM_PCH=n
WATER_LLVM_LINKER=auto
LLVM_LINK_JOBS=2
WATER_COMPILER_CACHE=none
EOF
    for name in llvm_bootstrap_cflags llvm_bootstrap_cxxflags llvm_compiler_target_triple \
                llvm_clangxx_works llvm_validate_darwin_host_compilers \
                llvm_bootstrap_config_signature
    do
        awk -v name="$name" '
            $0 == name "()" { copying=1 }
            copying { print }
            copying && /^}$/ { exit }
        ' "$root/build.sh"
    done
    cat <<'EOF'
die() { echo "$*" >&2; exit 1; }
select_llvm_targets() { echo 'AArch64;X86'; }
select_llvm_cache() { echo ''; }
darwin_sdkroot() { echo /test/SDKs/MacOSX.sdk; }
find_llvm_bootstrap_compiler() {
    case "$2" in
        clang) echo "$WHP_TEST_DIR/bin/cc" ;;
        clang++) echo "$WHP_TEST_DIR/bin/cxx" ;;
        *) exit 2 ;;
    esac
}
select_llvm_lld_backends() { echo 'COFF;MinGW'; }
host_lld_path() { return 1; }
case "$1" in
    signature) llvm_bootstrap_config_signature ;;
    flags) printf 'C=%s\nCXX=%s\n' "$(llvm_bootstrap_cflags)" "$(llvm_bootstrap_cxxflags)" ;;
    validate) llvm_validate_darwin_host_compilers "$WHP_TEST_DIR/bin/cc" "$WHP_TEST_DIR/bin/cxx" ;;
    clangxx) llvm_clangxx_works "$WHP_TEST_DIR/bin/cxx" ;;
    *) exit 2 ;;
esac
EOF
} > "$tmp/runner"
sh -n "$tmp/runner"

baseline=$(sh "$tmp/runner" signature)
printf '%s\n' "$baseline" | grep -Fq 'LLVM_BUILD_ARCH=arm64'
[ "$baseline" = "$(sh "$tmp/runner" signature)" ] || exit 1
[ "$baseline" != "$(MOCK_HOST_ARCH=x86_64 sh "$tmp/runner" signature)" ] || exit 1
[ "$baseline" != "$(MOCK_CXX_TRIPLE=x86_64-apple-darwin23 sh "$tmp/runner" signature)" ] || exit 1
[ "$baseline" != "$(MACOSX_DEPLOYMENT_TARGET=14.0 sh "$tmp/runner" signature)" ] || exit 1
[ "$baseline" != "$(CXXFLAGS_FOR_BUILD=-O0 sh "$tmp/runner" signature)" ] || exit 1
[ "$baseline" != "$(CFLAGS_FOR_BUILD=-O0 sh "$tmp/runner" signature)" ] || exit 1

flags=$(CFLAGS='-target arm64ec-pc-windows-msvc' \
        CXXFLAGS='-target arm64ec-pc-windows-msvc' \
        CFLAGS_FOR_BUILD='-O2 -DHOST_C=1' CXXFLAGS_FOR_BUILD='-O1 -DHOST_CXX=1' \
        sh "$tmp/runner" flags)
printf '%s\n' "$flags" | grep -Fq HOST_C=1
printf '%s\n' "$flags" | grep -Fq HOST_CXX=1
if printf '%s\n' "$flags" | grep -Fq arm64ec-pc-windows; then
    echo 'PE flags contaminated host LLVM bootstrap' >&2
    exit 1
fi

sh "$tmp/runner" validate
if MOCK_CXX_TRIPLE=arm64ec-pc-windows-msvc sh "$tmp/runner" validate >"$tmp/log" 2>&1; then
    echo 'Windows compiler accepted as macOS bootstrap' >&2
    exit 1
fi
grep -Fq 'rather than macOS' "$tmp/log"
if MOCK_CXX_TRIPLE=x86_64-apple-darwin23 sh "$tmp/runner" validate >"$tmp/log" 2>&1; then
    echo 'mismatched host C/C++ architectures accepted' >&2
    exit 1
fi
grep -Fq 'different macOS architectures' "$tmp/log"
sh "$tmp/runner" clangxx
if MOCK_CXX_FAIL=139 sh "$tmp/runner" clangxx >/dev/null 2>&1; then
    echo 'crashing clang++ accepted' >&2
    exit 1
fi
grep -Fq '"-DCMAKE_C_FLAGS=$(llvm_bootstrap_cflags)"' "$root/build.sh"
grep -Fq '"-DCMAKE_CXX_FLAGS=$(llvm_bootstrap_cxxflags)"' "$root/build.sh"
grep -Fq 'llvm_clangxx_works "$LLVM_BOOTSTRAP_DIR/bin/clang++" || return 0' "$root/build.sh"
echo 'PASS: LLVM host ABI cache identity, host flags and C++ crash guard'
