#!/bin/sh
# Inspect the i386 Microsoft C++ ABI before opting into LLVM libc++.
# This is a compile/symbol contract test, not a full Water PE runtime test.
set -eu

root=${WATER_SOURCE_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)}
cxx=${TEST_CLANGXX:-clang++}
nm=${TEST_LLVM_NM:-llvm-nm}

for tool in "$cxx" "$nm"; do
    command -v "$tool" >/dev/null 2>&1 || { echo "SKIP: $tool unavailable"; exit 0; }
done

# Water deliberately does not export the float hyperbolic CRT entry points on i386.
for spec in "$root/dlls/msvcrt/msvcrt.spec" "$root/dlls/ucrtbase/ucrtbase.spec"; do
    [ -f "$spec" ] || { echo "missing CRT spec: $spec" >&2; exit 1; }
    for func in sinhf coshf tanhf; do
        grep -Eq "^[[:space:]]*@ cdecl -arch=!i386 ${func}\\(float\\)" "$spec" ||
            { echo "i386 $func export policy changed in $spec" >&2; exit 1; }
    done
done

# Check that the pinned libc++ still contains the i386 MSVCRT wrappers.
header=${WHP_LLVM_SOURCE_DIR:-"$root/toolchains/llvm-project"}/libcxx/include/__math/hyperbolic_functions.h
if [ -f "$header" ]; then
    branches=$(grep -Fc 'defined(_LIBCPP_MSVCRT) && defined(__i386__)' "$header" || true)
    [ "$branches" -eq 3 ] || { echo "libc++ i386 math ABI guards changed" >&2; exit 1; }
    for func in sinh cosh tanh; do
        grep -Fq "__builtin_${func}(static_cast<double>(__x))" "$header" ||
            { echo "libc++ i386 $func wrapper changed" >&2; exit 1; }
    done
else
    echo 'NOTE: LLVM source not checked out; header guard inspection skipped' >&2
fi

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-i386-libcxx.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/probe.cpp" <<'CPP'
#if !defined(_WIN32) || !defined(_MSC_VER) || !defined(__i386__) || defined(__MINGW32__)
#error i386 Microsoft C++ ABI target is required
#endif
static_assert(sizeof(void *) == 4, "i386 pointer size");
static_assert(sizeof(wchar_t) == 2, "Windows wchar_t size");
float math_probe(float x)
{
    return static_cast<float>(__builtin_sinh(static_cast<double>(x)) +
                              __builtin_cosh(static_cast<double>(x)) +
                              __builtin_tanh(static_cast<double>(x)));
}
struct left { virtual ~left(); };
struct right { virtual ~right(); };
struct derived : left, right {};
right *rtti_probe(left *p) { return dynamic_cast<right *>(p); }
void throw_probe() { throw 7; }
int catch_probe()
{
    try { throw_probe(); }
    catch (int n) { return n; }
    return -1;
}
CPP
"$cxx" -target i686-pc-windows-msvc -fshort-wchar -fexceptions -frtti \
    -O0 -c "$tmp/probe.cpp" -o "$tmp/probe.obj"
"$nm" --undefined-only "$tmp/probe.obj" > "$tmp/undefined"

for symbol in _sinh _cosh _tanh ___RTDynamicCast ___CxxFrameHandler3 '__CxxThrowException@8'; do
    grep -Fq "$symbol" "$tmp/undefined" ||
        { echo "missing i386 ABI reference: $symbol" >&2; exit 1; }
done
if grep -Eq '(^|[[:space:]])_(sinhf|coshf|tanhf)$' "$tmp/undefined"; then
    echo 'i386 libc++ math probe references unavailable float CRT exports' >&2
    exit 1
fi
printf 'PASS: i386 MSVC math, pointer, wchar_t, RTTI and exception ABI references\n'
