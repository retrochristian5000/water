#!/bin/sh
# Self-contained contract tests for candidate modern LLVM PE libunwind archives.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
checker="$root/scripts/check-unwind-provider.sh"
clang=clang
ar=llvm-ar
nm=llvm-nm
for tool in "$clang" "$ar" "$nm"; do
    command -v "$tool" >/dev/null 2>&1 || { echo "SKIP: $tool unavailable"; exit 0; }
done
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

cat > "$tmp/probe.c" <<'EOF'
void _Unwind_RaiseException(void) {}
void _Unwind_Resume(void) {}
void __register_frame_info_bases(void) {}
void __deregister_frame_info_bases(void) {}
EOF
"$clang" -target x86_64-w64-windows-gnu -c "$tmp/probe.c" -o "$tmp/x64.o"
"$clang" -target i686-w64-windows-gnu -c "$tmp/probe.c" -o "$tmp/x86.o"
"$ar" rcs "$tmp/x64.a" "$tmp/x64.o"
"$ar" rcs "$tmp/x86.a" "$tmp/x86.o"
"$checker" "$tmp/x64.a" x86_64-w64-windows-gnu "$ar" "$nm"
"$checker" "$tmp/x86.a" i686-w64-windows-gnu "$ar" "$nm"

if "$checker" "$tmp/x86.a" x86_64-w64-windows-gnu "$ar" "$nm" >/dev/null 2>&1; then
    echo 'accepted wrong architecture' >&2; exit 1
fi
if "$checker" "$tmp/x64.a" x86_64-pc-windows-msvc "$ar" "$nm" >/dev/null 2>&1; then
    echo 'accepted MSVC exception ABI' >&2; exit 1
fi
if "$checker" "$tmp/x64.a" arm64ec-pc-windows-msvc "$ar" "$nm" >/dev/null 2>&1; then
    echo 'accepted ARM64EC exception ABI' >&2; exit 1
fi
printf 'void _Unwind_RaiseException(void) {}\n' > "$tmp/incomplete.c"
"$clang" -target x86_64-w64-windows-gnu -c "$tmp/incomplete.c" -o "$tmp/incomplete.o"
"$ar" rcs "$tmp/incomplete.a" "$tmp/incomplete.o"
if "$checker" "$tmp/incomplete.a" x86_64-w64-windows-gnu "$ar" "$nm" >/dev/null 2>&1; then
    echo 'accepted missing entry points' >&2; exit 1
fi
"$ar" rcs "$tmp/mixed.a" "$tmp/x86.o" "$tmp/x64.o"
if "$checker" "$tmp/mixed.a" x86_64-w64-windows-gnu "$ar" "$nm" >/dev/null 2>&1; then
    echo 'accepted mixed architecture' >&2; exit 1
fi
mkdir "$tmp/a" "$tmp/b"
cp "$tmp/x64.o" "$tmp/a/same.o"
cp "$tmp/x86.o" "$tmp/b/same.o"
"$ar" qc "$tmp/duplicate.a" "$tmp/a/same.o" "$tmp/b/same.o"
if "$checker" "$tmp/duplicate.a" x86_64-w64-windows-gnu "$ar" "$nm" >/dev/null 2>&1; then
    echo 'accepted duplicate member names' >&2; exit 1
fi
echo 'PASS: LLVM libunwind candidate checker'
