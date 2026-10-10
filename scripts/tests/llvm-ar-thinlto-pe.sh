#!/bin/sh
# Compile/link COFF archives with ThinLTO and prove the symbol-index contract.
# Full Water PE builds and ARM64EC hybrid archiving require separate testing.
set -eu
clang=${TEST_CLANG:-clang}
ar=${TEST_LLVM_AR:-llvm-ar}
ranlib=${TEST_LLVM_RANLIB:-llvm-ranlib}
link=${TEST_LLD_LINK:-lld-link}
for tool in "$clang" "$ar" "$ranlib" "$link"; do
    command -v "$tool" >/dev/null 2>&1 || { echo "SKIP: $tool unavailable"; exit 0; }
done
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-llvm-ar-thinlto.XXXXXX")
trap 'rm -rf "$tmp"' 0 HUP INT TERM
cat > "$tmp/entry.c" <<'C'
extern int whp_crt_helper(int);
volatile int whp_entry_result;
void mainCRTStartup(void) { whp_entry_result = whp_crt_helper(7); }
C
cat > "$tmp/crt.c" <<'C'
extern int whp_native_helper(int);
__attribute__((noinline)) int whp_crt_helper(int x) { return whp_native_helper(x) + 1; }
C
cat > "$tmp/native.c" <<'C'
__attribute__((noinline)) int whp_native_helper(int x) { return x * 3; }
C
for mapping in 'i686-pc-windows-msvc:x86' 'x86_64-pc-windows-msvc:x64' 'aarch64-pc-windows-msvc:arm64'; do
    target=${mapping%%:*}
    machine=${mapping#*:}
    "$clang" -target "$target" -O2 -flto=thin -c "$tmp/entry.c" -o "$tmp/$machine-entry.obj"
    "$clang" -target "$target" -O2 -flto=thin -c "$tmp/crt.c" -o "$tmp/$machine-crt.obj"
    "$clang" -target "$target" -O2 -c "$tmp/native.c" -o "$tmp/$machine-native.obj"
    archive="$tmp/$machine-libwinecrt0.a"
    "$ar" --format=coff rcs "$archive" "$tmp/$machine-crt.obj"
    "$ar" rs "$archive" "$tmp/$machine-native.obj"
    members=$("$ar" t "$archive")
    printf '%s\n' "$members" | grep -Fx "$machine-crt.obj" >/dev/null
    printf '%s\n' "$members" | grep -Fx "$machine-native.obj" >/dev/null
    "$link" /nologo /machine:"$machine" /subsystem:console /entry:mainCRTStartup \
        /nodefaultlib /opt:ref /out:"$tmp/$machine-late.exe" "$tmp/$machine-entry.obj" "$archive"
    "$link" /nologo /machine:"$machine" /subsystem:console /entry:mainCRTStartup \
        /nodefaultlib /opt:ref /out:"$tmp/$machine-early.exe" "$archive" "$tmp/$machine-entry.obj"
    echo "PASS: $target ThinLTO resolves llvm-ar archives at both positions"
done
"$ar" --format=coff rcS "$tmp/no-index.a" "$tmp/x64-crt.obj" "$tmp/x64-native.obj"
if "$link" /nologo /machine:x64 /subsystem:console /entry:mainCRTStartup \
        /nodefaultlib /opt:ref /out:"$tmp/no-index.exe" "$tmp/x64-entry.obj" "$tmp/no-index.a" >/dev/null 2>&1; then
    echo 'FAIL: archive without index linked' >&2
    exit 1
fi
"$ranlib" "$tmp/no-index.a"
"$link" /nologo /machine:x64 /subsystem:console /entry:mainCRTStartup \
    /nodefaultlib /opt:ref /out:"$tmp/with-index.exe" "$tmp/x64-entry.obj" "$tmp/no-index.a"
echo 'PASS: missing index breaks ThinLTO archive lookup; llvm-ranlib repairs it'
