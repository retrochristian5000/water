#!/bin/sh
# Validate Microsoft /EHc semantics with Clang's canonical code-generation flags.
# Baseline and ARM64EC must agree on external C function unwind assumptions.
set -eu
cc=${WATER_TEST_CLANGXX:-clang++}
command -v "$cc" >/dev/null 2>&1 || exit 77
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-ehc.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
cat >"$tmp/eh.cpp" <<'SRC'
extern "C" void whp_c_api();
struct Guard { ~Guard(); };
void whp_cpp_entry() { Guard g; whp_c_api(); }
SRC
for triple in i686-pc-windows-msvc x86_64-pc-windows-msvc aarch64-pc-windows-msvc arm64ec-pc-windows-msvc; do
    "$cc" --target="$triple" -S -emit-llvm -fexceptions -fcxx-exceptions \
        -o "$tmp/ehs.ll" "$tmp/eh.cpp"
    "$cc" --target="$triple" -S -emit-llvm -fexceptions -fcxx-exceptions \
        -Xclang -fexternc-nounwind -o "$tmp/ehsc.ll" "$tmp/eh.cpp"
    grep -F 'invoke void @whp_c_api()' "$tmp/ehs.ll" >/dev/null || {
        echo "$triple: /EHs must preserve C-call exceptional edge" >&2; exit 1;
    }
    grep -F 'call void @whp_c_api()' "$tmp/ehsc.ll" >/dev/null || {
        echo "$triple: /EHsc should allow direct C call" >&2; exit 1;
    }
    if grep -F 'invoke void @whp_c_api()' "$tmp/ehsc.ll" >/dev/null; then
        echo "$triple: /EHsc retained unexpected exceptional edge" >&2; exit 1;
    fi
    grep -F 'attributes #2 = { nounwind }' "$tmp/ehsc.ll" >/dev/null || {
        echo "$triple: /EHsc missing nounwind call attribute" >&2; exit 1;
    }
    # /EHc must not alter the target triple, architecture-specific symbol
    # naming or Microsoft C++ personality used under /EHs.
    grep -F '@__CxxFrameHandler3' "$tmp/ehs.ll" >/dev/null || {
        echo "$triple: C++ EH personality missing" >&2; exit 1;
    }
done
echo 'C++ extern-C unwinding /EHs vs /EHsc across x86 x64 ARM64 ARM64EC: pass'
