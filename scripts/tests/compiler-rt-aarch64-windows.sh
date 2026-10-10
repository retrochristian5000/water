#!/bin/sh
# Check AArch64 and ARM64EC Windows compiler-rt builtin contracts.
# Compile-time COFF inspection; full PE linking and runtime tests remain separate.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
llvm=${WHP_LLVM_SOURCE_DIR:-"$root/toolchains/llvm-project"}
recipe="$root/libs/compiler-rt/Makefile.in"
builtins="$llvm/compiler-rt/lib/builtins"
clang=${TEST_CLANG:-clang}
objdump=${TEST_LLVM_OBJDUMP:-llvm-objdump}

for name in aarch64/chkstk.S divti3.c modti3.c udivti3.c umodti3.c udivmodti4.c; do
    grep -Fq "lib/builtins/$name" "$recipe" || {
        echo "Water compiler-rt recipe omits $name" >&2
        exit 1
    }
    [ -f "$builtins/$name" ] || {
        echo "missing pinned LLVM compiler-rt source: $name" >&2
        exit 1
    }
done

for tool in "$clang" "$objdump"; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "SKIP: $tool not found" >&2
        exit 0
    }
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-compiler-rt-arm64.XXXXXX")
trap 'rm -rf "$tmp"' 0 HUP INT TERM

cat > "$tmp/consumer.c" <<'C'
#if !defined(_WIN32) || (!defined(__aarch64__) && !defined(__arm64ec__))
#error Expected an AArch64 Windows target
#endif
__attribute__((noinline))
__int128 whp_signed_remainder(__int128 a, __int128 b) { return a % b; }
__attribute__((noinline))
int whp_stack_probe(int i)
{
    volatile char stack[16384];
    stack[i & 8191] = (char)i;
    return stack[i & 8191];
}
C

# COFF undefined symbols are in section zero; definitions are in other sections.
require_symbol()
{
    object=$1
    symbol=$2
    section=$3
    if ! "$objdump" -t "$object" |
         grep -F " $symbol" |
         grep -Eq "\(sec[[:space:]]+$section\)"; then
        echo "missing $symbol in $object ($section)" >&2
        exit 1
    fi
}

for item in \
    'aarch64-pc-windows-msvc:__modti3:__chkstk' \
    'aarch64-w64-windows-gnu:__modti3:__chkstk' \
    'arm64ec-pc-windows-msvc:#__modti3:#__chkstk_arm64ec'
do
    target=${item%%:*}
    remaining=${item#*:}
    modulo=${remaining%%:*}
    stack=${remaining#*:}

    "$clang" -target "$target" -O0 -c "$tmp/consumer.c" -o "$tmp/consumer.obj"
    require_symbol "$tmp/consumer.obj" "$modulo" '0'
    require_symbol "$tmp/consumer.obj" "$stack" '0'

    # Compile the pinned LLVM implementation, not a local replacement shim.
    "$clang" -target "$target" -O0 -I"$builtins" \
        -c "$builtins/modti3.c" -o "$tmp/modti3.obj"
    "$objdump" -t "$tmp/modti3.obj" | grep -F " $modulo" |
        grep -Eq '\(sec[[:space:]]+[1-9][0-9]*\)' || {
            echo "LLVM compiler-rt did not define $modulo for $target" >&2
            exit 1
        }

    "$clang" -target "$target" -c "$builtins/aarch64/chkstk.S" -o "$tmp/chkstk.obj"
    "$objdump" -t "$tmp/chkstk.obj" | grep -F " $stack" |
        grep -Eq '\(sec[[:space:]]+[1-9][0-9]*\)' || {
            echo "LLVM compiler-rt did not define $stack for $target" >&2
            exit 1
        }
done

echo 'PASS: AArch64 Windows and ARM64EC compiler-rt builtin ABI contract'
