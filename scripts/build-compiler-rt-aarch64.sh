#!/bin/sh
# Build and audit an LLVM compiler-rt AArch64 Windows candidate, separately
# from Water's existing libcompiler-rt.a. This does not change link selection.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_dir=${WHP_LLVM_SOURCE_DIR:-"$root/toolchains/llvm-project"}
llvm_bin=${WHP_COMPILER_RT_LLVM_BIN:-}
if [ -z "$llvm_bin" ]; then
    if [ -n "${WHP_LLVM_PREFIX:-}" ]; then
        llvm_bin="$WHP_LLVM_PREFIX/bin"
    else
        llvm_bin="${WHP_LLVM_BUILD_DIR:-"$root/build/llvm-bootstrap"}/bin"
    fi
fi
build_root=${WHP_COMPILER_RT_BUILD_ROOT:-"$root/build/llvm-compiler-rt-aarch64"}
target=${WHP_COMPILER_RT_AARCH64_TARGET:-aarch64-pc-windows-msvc}

case "$target" in
    aarch64-windows) target=aarch64-pc-windows-msvc ;;
    aarch64-pc-windows-msvc|aarch64-w64-windows-gnu) ;;
    *) echo "unsupported AArch64 Windows compiler-rt target: $target" >&2; exit 2 ;;
esac

[ -f "$source_dir/compiler-rt/lib/builtins/CMakeLists.txt" ] || {
    echo "missing LLVM compiler-rt builtins source: $source_dir" >&2; exit 1;
}
for tool in clang clang++ llvm-ar llvm-ranlib llvm-nm; do
    [ -x "$llvm_bin/$tool" ] || {
        echo "missing LLVM tool: $llvm_bin/$tool" >&2; exit 1;
    }
done
cmake_cmd=${CMAKE:-cmake}
command -v "$cmake_cmd" >/dev/null 2>&1 || {
    echo "CMake is required for LLVM compiler-rt builtins" >&2; exit 1;
}

# Keep separate CMake caches for the two distinct Windows runtime ABIs.
build_dir="$build_root/$target"
mkdir -p "$build_dir"
set -- \
    -S "$source_dir/compiler-rt/lib/builtins" \
    -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_SYSTEM_NAME=Windows \
    "-DCMAKE_C_COMPILER=$llvm_bin/clang" \
    "-DCMAKE_CXX_COMPILER=$llvm_bin/clang++" \
    "-DCMAKE_ASM_COMPILER=$llvm_bin/clang" \
    "-DCMAKE_C_COMPILER_TARGET:STRING=$target" \
    "-DCMAKE_CXX_COMPILER_TARGET:STRING=$target" \
    "-DCMAKE_ASM_COMPILER_TARGET:STRING=$target" \
    "-DCMAKE_AR=$llvm_bin/llvm-ar" \
    "-DCMAKE_RANLIB=$llvm_bin/llvm-ranlib" \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    "-DCMAKE_C_FLAGS=-fno-rtlib-defaultlib -fms-omit-default-lib" \
    "-DCMAKE_CXX_FLAGS=-fno-rtlib-defaultlib -fms-omit-default-lib" \
    "-DCMAKE_ASM_FLAGS=-fno-rtlib-defaultlib" \
    -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON \
    -DCOMPILER_RT_BUILTINS_ENABLE_PIC=OFF \
    -DCOMPILER_RT_BUILTINS_HIDE_SYMBOLS=OFF \
    -DCOMPILER_RT_ENABLE_WINDOWS_AARCH64_CHKSTK=ON

if [ -n "${WHP_COMPILER_RT_LLVM_CMAKE_DIR:-}" ]; then
    set -- "$@" "-DLLVM_CMAKE_DIR=$WHP_COMPILER_RT_LLVM_CMAKE_DIR"
elif [ -f "$llvm_bin/../lib/cmake/llvm/LLVMConfig.cmake" ]; then
    set -- "$@" "-DLLVM_CMAKE_DIR=$llvm_bin/../lib/cmake/llvm"
elif [ -f "$llvm_bin/../lib64/cmake/llvm/LLVMConfig.cmake" ]; then
    set -- "$@" "-DLLVM_CMAKE_DIR=$llvm_bin/../lib64/cmake/llvm"
else
    echo "warning: no LLVM CMake package beside $llvm_bin; standalone source checks may fail" >&2
fi

"$cmake_cmd" "$@"
# Do not silently reuse an archive for another target triple.
grep -F "CMAKE_C_COMPILER_TARGET:STRING=$target" "$build_dir/CMakeCache.txt" >/dev/null || {
    echo "compiler-rt CMake target drift: expected $target" >&2; exit 1;
}
"$cmake_cmd" --build "$build_dir" --target builtins

archives=$(find "$build_dir" -type f \
    \( -name '*clang_rt.builtins*.a' -o -name '*clang_rt.builtins*.lib' \) -print)
case "$archives" in
    '') echo "no compiler-rt builtins archive was produced" >&2; exit 1 ;;
    *'
'*) echo "ambiguous compiler-rt builtins archives:" >&2
          printf '%s\n' "$archives" >&2; exit 1 ;;
esac

# Both allowed target triples must produce ARM64 (0xAA64) COFF members.
"$root/scripts/check-libcxx-archive.sh" "$archives" \
    aarch64-pc-windows-msvc "$llvm_bin/llvm-ar" compiler-rt

# Water currently depends on this Windows stack-probe symbol. The fork's
# opt-in CMake setting supplies it without depending on a system MSVC CRT.
symbols=$("$llvm_bin/llvm-nm" --defined-only "$archives") || {
    echo "llvm-nm failed to inspect compiler-rt builtins" >&2; exit 1;
}
printf '%s\n' "$symbols" | grep -E '(^|[[:space:]])__chkstk$' >/dev/null || {
    echo "compiler-rt AArch64 archive is missing __chkstk" >&2; exit 1;
}

provider="$build_dir/provider"
mkdir -p "$provider"
cp "$archives" "$provider/libcompiler-rt.a.tmp.$$"
mv -f "$provider/libcompiler-rt.a.tmp.$$" "$provider/libcompiler-rt.a"
printf 'Verified AArch64 Windows compiler-rt candidate: %s\n' "$provider/libcompiler-rt.a"
printf 'Not linked by Water yet; existing libs/compiler-rt remains active.\n'
