#!/bin/sh
# PowerPC Windows libc++ readiness gate: object format before runtime switch.
# This test is diagnostic. The known LLVM ELF-only result is a BLOCKED
# migration, not evidence that a PowerPC PE libc++ archive is usable.
set -eu

root=\${WATER_SOURCE_DIR:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)}
cxx=\${TEST_CLANGXX:-clang++}
triple=\${WATER_PPC_TEST_TRIPLE:-powerpcle-windows}

for f in "$root/build.sh" "$root/configure.ac" "$root/libs/c++/Makefile.in"; do
    [ -f "$f" ] || { echo "missing Water source: $f" >&2; exit 2; }
done
grep -Fq 'WATER_ARCH_POWERPC:powerpc' "$root/build.sh" ||
    { echo 'PowerPC architecture selection missing' >&2; exit 1; }
grep -Fq 'powerpc)' "$root/build.sh" ||
    { echo 'PowerPC libc++ provider boundary missing' >&2; exit 1; }
grep -Fq 'legacy provider retained pending PowerPC COFF runtime support' "$root/build.sh" ||
    { echo 'PowerPC legacy runtime fallback changed; audit provider before migration' >&2; exit 1; }
grep -Fq 'whether the PowerPC C++ target emits PE/COFF' "$root/configure.ac" ||
    { echo 'PowerPC configure object-format guard missing' >&2; exit 1; }

if ! command -v "$cxx" >/dev/null 2>&1; then
    echo "SKIP: no C++ cross-compiler found: $cxx" >&2
    exit 77
fi

tmp=$(mktemp -d "\${TMPDIR:-/tmp}/water-libcxx-ppc.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat > "$tmp/probe.cpp" <<'CPP'
#if !defined(__powerpc__) && !defined(__ppc__) && !defined(_M_PPC)
# error PowerPC compiler target required
#endif
#if !defined(_WIN32)
# error Windows target triple required
#endif
static_assert(sizeof(void *) == 4, "32-bit PowerPC Windows pointer ABI");
static_assert(sizeof(wchar_t) == 2, "Windows wchar_t ABI");
struct base { virtual ~base(); };
struct derived : base {};
extern "C" int whp_ppc_probe(base *p) { return dynamic_cast<derived *>(p) != nullptr; }
CPP

if ! "$cxx" -target "$triple" -fshort-wchar -std=c++17 \
    -fexceptions -frtti -c "$tmp/probe.cpp" -o "$tmp/probe.o" 2>"$tmp/compile.err"; then
    echo "BLOCKED: $triple could not compile the PowerPC Windows C++ ABI probe" >&2
    cat "$tmp/compile.err" >&2
    exit 0
fi

# A Windows PE/COFF PowerPC object must start with little-endian machine
# 0x01f0 (IMAGE_FILE_MACHINE_POWERPC) or 0x01f1 (POWERPCFP).
# Check bytes, not the target name or the misleading .o suffix.
bytes=$(od -An -tu1 -N4 "$tmp/probe.o" |
    tr -s '[:space:]' ':' | sed 's/^://;s/:$//')
case "$bytes" in
    240:1:*|241:1:*)
        echo "PASS: $triple emits genuine PowerPC PE/COFF C++ objects"
        ;;
    127:69:76:70)
        echo "BLOCKED: $triple emitted ELF despite a Windows target triple"
        echo 'Retain the older PowerPC libc++ until LLVM emits and links genuine PPC PE/COFF.'
        exit 0
        ;;
    *)
        echo "BLOCKED: $triple emitted an unsupported object header: $bytes" >&2
        exit 0
        ;;
esac

# Accepting a COFF object is only the first condition. Test that the
# PowerPC final linker exists before attempting the libc++ migration.
linker=\${WATER_PPC_TEST_LINKER:-lld-link}
if ! command -v "$linker" >/dev/null 2>&1; then
    echo "BLOCKED: no verified PowerPC Windows PE linker: $linker"
    exit 0
fi
cat > "$tmp/link.cpp" <<'CPP'
extern "C" __declspec(dllexport) int whp_ppc_link() { return 1; }
CPP
if ! "$cxx" -target "$triple" -fshort-wchar -c "$tmp/link.cpp" \
    -o "$tmp/link.o" 2>"$tmp/link-compile.err"; then
    echo 'BLOCKED: compiler failed minimal PowerPC PE export compilation'
    exit 0
fi
if ! "$linker" /machine:powerpc /dll /noentry "/out:$tmp/probe.dll" \
    "$tmp/link.o" >"$tmp/link.log" 2>&1; then
    echo 'BLOCKED: COFF target exists, but PowerPC Windows DLL linking is not ready'
    sed -n '1,8p' "$tmp/link.log"
    exit 0
fi
if [ ! -f "$tmp/probe.dll" ]; then
    echo 'BLOCKED: linker did not produce a PowerPC Windows DLL'
    exit 0
fi
echo 'READY FOR FURTHER AUDIT: PowerPC object and linker format passed.'
echo 'C++ runtime, exception unwinding, import libraries and modern libc++ are NOT yet validated.'
