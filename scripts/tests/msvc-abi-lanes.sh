#!/bin/sh
# Prevent Microsoft's vcruntime libc++ provider leaking into GNU/MinGW ABI.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-ms-abi-lanes.XXXXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

for name in selected_libcxx_archs selected_llvm_libcxx_archs \
            prepare_one_llvm_libcxx prepare_libcxx_provider
do
    awk -v name="$name" '
        $0 == name "()" { copy=1 }
        copy { print }
        copy && /^}$/ { exit }
    ' "$root/build.sh" >> "$tmp/funcs"
done
test "$(grep -c '^[_a-z].*()$' "$tmp/funcs")" -eq 4
sh -n "$tmp/funcs"

{
    cat <<EOF
#!/bin/sh
set -eu
TEST_ROOT='$tmp'
LLVM_LIBCXX_RUNTIME_DIR='$tmp/provider'
EOF
    cat <<'EOF'
WHP_CONFIGURE_ARCHS_SET=1
WHP_CONFIGURE_ARCHS=arm64ec
WATER_ARCHS_MODE=auto
WATER_LIBCXX=${WATER_LIBCXX:-llvm}
WATER_LIBCXX_LEGACY_POLICY=${WATER_LIBCXX_LEGACY_POLICY:-allow}
WATER_LIBCXX_I386=legacy
WHP_BASH_CMD=
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
EOF
    cat "$tmp/funcs"
    cat <<'EOF'
case "$1" in
    direct)
        prepare_one_llvm_libcxx arm64ec
        ;;
    select)
        # Count would-be Microsoft builds without compiling anything.
        prepare_one_llvm_libcxx() { printf '%s\n' "$1" >> "$TEST_ROOT/selected"; }
        prepare_libcxx_provider
        printf 'MODE=%s\nLEGACY=%s\nSTATE=%s\n' \
            "$WHP_LIBCXX_INSTALL_MODE" "$WHP_LIBCXX_INSTALL_LEGACY_HEADERS" \
            "$WHP_LIBCXX_STATE"
        ;;
    *) exit 2 ;;
esac
EOF
} > "$tmp/runner"
sh -n "$tmp/runner"

for mode in llvm-mingw y n
do
    rm -f "$tmp/selected"
    WATER_WITH_MINGW=$mode sh "$tmp/runner" select > "$tmp/out"
    grep -Fxq 'MODE=legacy' "$tmp/out"
    grep -Fxq 'LEGACY=1' "$tmp/out"
    grep -Fq 'msvc-provider=disabled' "$tmp/out"
    test ! -e "$tmp/selected" || {
        echo "Microsoft provider built in $mode mode" >&2
        exit 1
    }
    if WATER_WITH_MINGW=$mode WATER_LIBCXX_LEGACY_POLICY=error \
           sh "$tmp/runner" select > "$tmp/out" 2> "$tmp/err"; then
        echo "Strict provider policy accepted $mode" >&2
        exit 1
    fi
    grep -Fq 'GNU/MinGW or disabled PE mode' "$tmp/err"
    if WATER_WITH_MINGW=$mode sh "$tmp/runner" direct > "$tmp/out" 2> "$tmp/err"; then
        echo "Direct Microsoft runtime build bypassed $mode isolation" >&2
        exit 1
    fi
    grep -Fq 'Microsoft-ABI LLVM libc++ cannot be used' "$tmp/err"
done

for mode in auto clang
do
    rm -f "$tmp/selected"
    WATER_WITH_MINGW=$mode sh "$tmp/runner" select > "$tmp/out"
    grep -Fxq 'MODE=llvm' "$tmp/out"
    grep -Fxq 'LEGACY=0' "$tmp/out"
    grep -Fxq arm64ec "$tmp/selected"
    grep -Fxq x86_64 "$tmp/selected"
    test "$(wc -l < "$tmp/selected" | tr -d '[:space:]')" = 2
done

rm -f "$tmp/selected"
WATER_WITH_MINGW=llvm-mingw WATER_LIBCXX=legacy sh "$tmp/runner" select > "$tmp/out"
grep -Fxq 'MODE=legacy' "$tmp/out"
grep -Fxq 'LEGACY=1' "$tmp/out"
test ! -e "$tmp/selected"

grep -Fq '*mingw*|*-windows-gnu*)' "$root/configure.ac"
grep -Fq '*-lwhp-libcxx*)' "$root/configure.ac"

if command -v clang++ >/dev/null 2>&1; then
    printf 'struct T { virtual int f(); }; int T::f() { return 1; }\n' > "$tmp/abi.cc"
    if clang++ --target=arm64ec-pc-windows-msvc -S -emit-llvm \
            "$tmp/abi.cc" -o "$tmp/ms.ll" >/dev/null 2>&1 &&
       clang++ --target=arm64ec-w64-windows-gnu -S -emit-llvm \
            "$tmp/abi.cc" -o "$tmp/gnu.ll" >/dev/null 2>&1
    then
        grep -Fq '?f@T@@' "$tmp/ms.ll"
        grep -Fq '_ZN1T1fEv' "$tmp/gnu.ll"
    fi
fi

echo 'PASS: Microsoft vs GNU/MinGW libc++ provider and ARM64EC ABI lanes'
