#!/bin/sh
# ARM64EC bitcode/COFF regression. This is a link-only test, not a runnable PE.
# Uses synthetic OS thunk providers only to isolate the linker from the Windows SDK.
set -eu
cc=${TEST_CLANG:-clang}
cxx=${TEST_CLANGXX:-clang++}
ar=${TEST_LLVM_AR:-llvm-ar}
link=${TEST_LLD_LINK:-lld-link}
for tool in "$cc" "$cxx" "$ar" "$link"; do
    command -v "$tool" >/dev/null 2>&1 || { echo "SKIP: $tool unavailable"; exit 0; }
done
work=$(mktemp -d "${TMPDIR:-/tmp}/water-arm64ec-thinlto.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
target=arm64ec-pc-windows-msvc
cat > "$work/runtime.c" <<'C'
/* Definitions satisfy Windows ARM64EC dispatch thunk *references* at link time.
 * They are not implementations and must never be executed. */
void __os_arm64x_dispatch_ret(void) {}
void __os_arm64x_dispatch_call_no_redirect(void) {}
void __os_arm64x_check_icall(void) {}
void __icall_helper_arm64ec(void) {}
C
cat > "$work/entry.c" <<'C'
extern int whp_crt_helper(int);
volatile int whp_result;
void mainCRTStartup(void) { whp_result = whp_crt_helper(7); }
C
cat > "$work/helper.c" <<'C'
extern int whp_native_helper(int);
__attribute__((noinline)) int whp_crt_helper(int n) { return whp_native_helper(n) + 1; }
C
cat > "$work/native.c" <<'C'
__attribute__((noinline)) int whp_native_helper(int n) { return n * 3; }
C
cat > "$work/new_entry.cpp" <<'CXX'
extern "C" void mainCRTStartup();
void *volatile whp_cpp_result;
extern "C" void mainCRTStartup() { whp_cpp_result = new int[11]; }
CXX
cat > "$work/new_provider.cpp" <<'CXX'
static unsigned char arena[128];
void *operator new[](unsigned long long) { return arena; }
CXX
"$cc" -target "$target" -O0 -c "$work/runtime.c" -o "$work/runtime.obj"
"$cc" -target "$target" -O2 -c "$work/native.c" -o "$work/native.obj"
"$cxx" -target "$target" -O2 -c "$work/new_provider.cpp" -o "$work/new_provider.obj"
# Native baseline MUST work first: otherwise toolchain/SDK failure is unrelated to LTO.
"$cc" -target "$target" -O2 -c "$work/entry.c" -o "$work/baseline-entry.obj"
"$cc" -target "$target" -O2 -c "$work/helper.c" -o "$work/baseline-helper.obj"
"$link" /nologo /machine:arm64ec /subsystem:console /entry:mainCRTStartup /nodefaultlib /opt:ref \
    /out:"$work/baseline.exe" "$work/baseline-entry.obj" "$work/baseline-helper.obj" \
    "$work/native.obj" "$work/runtime.obj"
echo 'PASS: ARM64EC native COFF baseline'
for mode in thin full; do
    case "$mode" in thin) flag=-flto=thin ;; full) flag=-flto=full ;; esac
    "$cc" -target "$target" -O2 "$flag" -c "$work/entry.c" -o "$work/$mode-entry.obj"
    "$cc" -target "$target" -O2 "$flag" -c "$work/helper.c" -o "$work/$mode-helper.obj"
    # Separate BitcodeFile machine/ABI handling from archive symbol-index handling.
    "$link" /nologo /machine:arm64ec /subsystem:console /entry:mainCRTStartup \
        /nodefaultlib /opt:ref /out:"$work/$mode-direct.exe" \
        "$work/$mode-entry.obj" "$work/$mode-helper.obj" "$work/native.obj" "$work/runtime.obj"
    echo "PASS: ARM64EC $mode direct bitcode link"
    "$ar" --format=coff rcs "$work/$mode-helper.a" "$work/$mode-helper.obj" "$work/native.obj"
    # Both archive positions must be equivalent.
    for order in late early; do
        if [ "$order" = late ]; then
            set -- "$work/$mode-entry.obj" "$work/$mode-helper.a"
        else
            set -- "$work/$mode-helper.a" "$work/$mode-entry.obj"
        fi
        "$link" /nologo /machine:arm64ec /subsystem:console /entry:mainCRTStartup \
            /nodefaultlib /opt:ref /out:"$work/$mode-$order.exe" "$@" "$work/runtime.obj"
        echo "PASS: ARM64EC $mode archive position $order"
    done
done
# Reproduce ARM64EC C++ allocation via an archive, as with the real CRT.
"$ar" --format=coff rcs "$work/alloc.a" "$work/new_provider.obj"
"$cxx" -target "$target" -O2 -c "$work/new_entry.cpp" -o "$work/native-new_entry.obj"
"$link" /nologo /machine:arm64ec /subsystem:console /entry:mainCRTStartup \
    /nodefaultlib /opt:ref /out:"$work/native-new.exe" "$work/native-new_entry.obj" \
    "$work/alloc.a" "$work/runtime.obj"
echo 'PASS: ARM64EC native C++ allocation baseline'
for mode in thin full; do
    case "$mode" in thin) flag=-flto=thin ;; full) flag=-flto=full ;; esac
    "$cxx" -target "$target" -O2 "$flag" -c "$work/new_entry.cpp" -o "$work/$mode-new_entry.obj"
    "$link" /nologo /machine:arm64ec /subsystem:console /entry:mainCRTStartup \
        /nodefaultlib /opt:ref /out:"$work/$mode-new.exe" "$work/$mode-new_entry.obj" \
        "$work/alloc.a" "$work/runtime.obj"
    echo "PASS: ARM64EC $mode C++ operator new[] resolves"
done
