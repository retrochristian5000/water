#!/bin/sh
# Test MSVC /Gv decoration on x86/x64 and the unsupported ARM64EC ABI.
# Optional argument: the built Water winebuild binary for DEF round-tripping.
set -eu
for tool in clang-cl llvm-objdump lld-link; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "/Gv test skipped: missing $tool" >&2
        exit 77
    }
done
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat > "$tmp/producer.c" <<'EOF'
int _fltused = 0;
int whp_vector(int a, double b) { return a + (int)b; }
int __cdecl whp_cdecl(int a, double b) { return a + (int)b; }
int whp_variadic(int a, ...) { return a; }
int main(void) { return 0; }
EOF
cat > "$tmp/consumer.c" <<'EOF'
int _fltused = 0;
__declspec(dllimport) int __vectorcall whp_vector(int, double);
int whp_client(int a, double b) { return whp_vector(a, b); }
EOF

check_obj()
{
    triple=$1
    name=$2
    vector=$3
    ordinary=$4
    clang-cl "--target=$triple" /nologo /Gv /c "$tmp/producer.c" "/Fo$tmp/$name.obj"
    llvm-objdump --syms "$tmp/$name.obj" > "$tmp/$name.syms"
    grep -F "$vector" "$tmp/$name.syms" >/dev/null || {
        echo "missing expected vector symbol $vector" >&2; exit 1;
    }
    grep -F "$ordinary" "$tmp/$name.syms" >/dev/null || {
        echo "missing explicit cdecl symbol $ordinary" >&2; exit 1;
    }
}
check_obj i686-pc-windows-msvc x86 'whp_vector@@12' '_whp_cdecl'
check_obj x86_64-pc-windows-msvc x64 'whp_vector@@16' 'whp_cdecl'
check_obj aarch64-pc-windows-msvc arm64 'whp_vector' 'whp_cdecl'
check_obj arm64ec-pc-windows-msvc ec '#whp_vector' '#whp_cdecl'

for machine in x86 x64; do
    if [ "$machine" = x86 ]; then
        triple=i686-pc-windows-msvc
        size=12
    else
        triple=x86_64-pc-windows-msvc
        size=16
    fi
    printf 'LIBRARY whp.dll\nEXPORTS\nwhp_vector@@%s\n' "$size" > "$tmp/$machine.def"
    lld-link /nologo /dll /noentry /nodefaultlib "/machine:$machine" \
        "/def:$tmp/$machine.def" "/out:$tmp/$machine.dll" \
        "/implib:$tmp/$machine.lib" "$tmp/$machine.obj"
    clang-cl "--target=$triple" /nologo /c "$tmp/consumer.c" "/Fo$tmp/$machine-client.obj"
    llvm-objdump --syms "$tmp/$machine-client.obj" > "$tmp/$machine-client.syms"
    grep -F "__imp_whp_vector@@$size" "$tmp/$machine-client.syms" >/dev/null
    lld-link /nologo /dll /noentry /nodefaultlib "/machine:$machine" \
        "/out:$tmp/$machine-client.dll" "$tmp/$machine-client.obj" "$tmp/$machine.lib"
done

# Microsoft STL needs to parse vectorcall function-pointer types on EC,
# but ARM64EC code generation for the convention remains unsupported.
cat > "$tmp/type.c" <<'EOF'
typedef int (__vectorcall *whp_pointer_type)(int, double);
whp_pointer_type whp_pointer;
EOF
clang-cl --target=arm64ec-pc-windows-msvc /nologo /c "$tmp/type.c" "/Fo$tmp/type.obj"
printf 'int __vectorcall unsupported(int a, double b) { return a; }\n' > "$tmp/unsupported.c"
if clang-cl --target=arm64ec-pc-windows-msvc /nologo /c \
    "$tmp/unsupported.c" "/Fo$tmp/unsupported.obj" >"$tmp/unsupported.log" 2>&1; then
    echo "ARM64EC vectorcall unexpectedly generated code" >&2
    exit 1
fi
grep -F '__vectorcall calling convention is not currently supported' \
    "$tmp/unsupported.log" >/dev/null

if [ "$#" -ge 1 ]; then
    winebuild=$1
    cat > "$tmp/exports.def" <<'EOF'
LIBRARY whp.dll
EXPORTS
    whp_vector@@12
    whp_stdcall@8
EOF
    "$winebuild" --target=i386-pc-windows --def \
        -E "$tmp/exports.def" -o "$tmp/roundtrip-x86.def"
    grep -E '^  whp_vector@@12( |$)' "$tmp/roundtrip-x86.def" >/dev/null
    grep -E '^  whp_stdcall@8( |$)' "$tmp/roundtrip-x86.def" >/dev/null
    "$winebuild" --target=x86_64-pc-windows --def \
        -E "$tmp/x64.def" -o "$tmp/roundtrip-x64.def"
    grep -F 'whp_vector@@16' "$tmp/roundtrip-x64.def" >/dev/null
fi
echo '/Gv symbol and import-library ABI, ARM64EC boundary: pass'
