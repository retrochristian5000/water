#!/bin/sh
# Native C bridge regression; does not require building LLVM or Water.
set -eu
root=$(CDPATH= cd -P -- "$(dirname -- "$0")/../.." && pwd -P)
bridge=$root/cc.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-cc-test.XXXXXXXX")
trap 'rm -rf -- "$tmp"' 0
trap 'exit 1' 1 2 3 15

sh -n "$bridge"
sh -n "$root/build.sh"
grep -Fq 'prepare_native_cc_seed' "$root/build.sh"
cc=$(WHP_CC_SEED= CC_FOR_BUILD= sh "$bridge" --print-cc)
[ -x "$cc" ]
[ "$(WHP_CC_SEED="$cc" sh "$bridge" --print-cc)" = "$cc" ]
[ "$(CC_FOR_BUILD="$cc" WHP_CC_SEED= sh "$bridge" --print-cc)" = "$cc" ]

printf '%s\n' 'int main(void) { return 0; }' > "$tmp/test.c"
WHP_CC_SEED="$cc" sh "$bridge" "$tmp/test.c" -o "$tmp/native"
"$tmp/native"

mkdir -p "$tmp/path with spaces"
cp "$bridge" "$tmp/path with spaces/cc.sh"
[ "$(WHP_CC_SEED="$cc" sh "$tmp/path with spaces/cc.sh" --print-cc)" = "$cc" ]

if WHP_CC_SEED="$bridge" sh "$bridge" --print-cc > "$tmp/log" 2>&1; then
    echo 'recursive seed adapter accepted' >&2; exit 1
fi
grep -Fq 'resolves to an adapter' "$tmp/log"

if WHP_CC_SEED=/no/such/compiler sh "$bridge" --print-cc > "$tmp/log" 2>&1; then
    echo 'missing explicit compiler accepted' >&2; exit 1
fi
grep -Fq 'not executable' "$tmp/log"

cat > "$tmp/pe-compiler" <<'PE'
#!/bin/sh
out=
while [ "$#" -gt 0 ]; do
    if [ "$1" = -o ]; then shift; out=$1; fi
    shift
done
[ -n "$out" ] || exit 1
printf 'MZ Not a native executable' > "$out"
PE
chmod +x "$tmp/pe-compiler"
if WHP_CC_SEED="$tmp/pe-compiler" sh "$bridge" --print-cc > "$tmp/log" 2>&1; then
    echo 'fake PE compiler accepted as native' >&2; exit 1
fi
grep -Fq 'native C executable failed' "$tmp/log"

# The seed probe must ignore target flags inherited from PE builds.
[ "$(WHP_CC_SEED="$cc" CFLAGS='-target arm64ec-pc-windows-msvc' \
    LDFLAGS='-Wl,/machine:arm64ec' sh "$bridge" --print-cc)" = "$cc" ]
if WHP_CC_SEED="$cc" sh "$bridge" --cxx > "$tmp/log" 2>&1; then
    echo 'C-only adapter accepted C++ mode' >&2; exit 1
fi
echo 'PASS: native cc.sh selection, execute probe, overrides, recursion and PE isolation'
