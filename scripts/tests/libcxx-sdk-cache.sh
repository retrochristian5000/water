#!/bin/sh
# Execute Water's own SDK-projection helper on disposable COM/Windows headers.
# An LLVM rebuild is intentionally not required.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-libcxx-sdk-cache.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

mkdir -p "$tmp/source/include" "$tmp/cache"
for header in windef.h guiddef.h winnt.h basetsd.h windows.h winbase.h objbase.h combaseapi.h
do
    printf '/* %s baseline */\n' "$header" > "$tmp/source/include/$header"
done
printf '%s\n' '/* base IDL */' > "$tmp/source/include/wtypesbase.idl"
printf '%s\n' 'import "wtypesbase.idl";' > "$tmp/source/include/wtypes.idl"
cat > "$tmp/source/include/unknwn.idl" <<'IDL'
import "wtypes.idl";
uuid(00000000-0000-0000-C000-000000000046)
virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObject) = 0;
virtual ULONG STDMETHODCALLTYPE AddRef(void) = 0;
virtual ULONG STDMETHODCALLTYPE Release(void) = 0;
IDL

awk '/^prepare_llvm_windows_sdk_headers\(\)$/ { copying=1 }
     copying { print }
     copying && /^}$/ { exit }' "$root/build.sh" > "$tmp/function"
grep -Fq 'WINDOWS_SDK_HEADERS=$whp_win_sdk_public_state' "$tmp/function" ||
    { echo 'libc++ SDK header fingerprint wiring is missing' >&2; exit 1; }

{
    printf '%s\n' '#!/bin/sh' 'set -eu' \
        'SOURCE_DIR=$1' 'LLVM_LIBCXX_RUNTIME_DIR=$2' \
        'LLVM_LIBCXX_RECIPE=16' \
        'die() { printf "%s\n" "$*" >&2; exit 1; }'
    cat "$tmp/function"
    printf '%s\n' 'prepare_llvm_windows_sdk_headers'
} > "$tmp/run"
sh -n "$tmp/run"
run() { sh "$tmp/run" "$tmp/source" "$tmp/cache" > "$tmp/log"; }

run
sdk="$tmp/cache/windows-sdk-headers"
[ -f "$sdk/wtypes.h" ] && [ -f "$sdk/unknwn.h" ]
baseline=$(cat "$sdk/.whp-state")
# On an unchanged source, the overlay must be cached, not unnecessarily
# recreated. A disposable sentinel makes this observable without timestamps.
printf 'untouched\n' > "$sdk/sentinel"
run
[ -f "$sdk/sentinel" ] || { echo 'unchanged SDK needlessly rebuilt' >&2; exit 1; }
[ "$(cat "$sdk/.whp-state")" = "$baseline" ]

# Windows ARM64 field-layout fixes must invalidate the libc++ archive.
printf '/* new ARM64 ABI field */\n' >> "$tmp/source/include/winnt.h"
run
[ ! -e "$sdk/sentinel" ] || { echo 'winnt.h change missed SDK cache' >&2; exit 1; }
winnt_state=$(cat "$sdk/.whp-state")
[ "$winnt_state" != "$baseline" ]

# COM declaration macro changes must likewise invalidate the cache.
printf '/* updated COM IID macro */\n' >> "$tmp/source/include/objbase.h"
run
[ "$(cat "$sdk/.whp-state")" != "$winnt_state" ] ||
    { echo 'objbase.h change missed SDK cache' >&2; exit 1; }

# Generated projection corruption must be repaired even with matching IDLs.
rm "$sdk/unknwn.h"
run
[ -f "$sdk/unknwn.h" ] || { echo 'deleted IUnknown projection survived' >&2; exit 1; }

# Preserve IDL provenance checks as additional cache dependencies.
idl_state=$(cat "$sdk/.whp-state")
printf '/* changed */\n' >> "$tmp/source/include/unknwn.idl"
run
[ "$(cat "$sdk/.whp-state")" != "$idl_state" ] ||
    { echo 'IUnknown IDL change missed SDK cache' >&2; exit 1; }

# An absent ABI header is a hard failure, not a stamped good cache.
rm "$tmp/source/include/guiddef.h"
if sh "$tmp/run" "$tmp/source" "$tmp/cache" > "$tmp/log" 2>&1; then
    echo 'missing GUID ABI header was silently accepted' >&2
    exit 1
fi
grep -Fq 'Water libc++ SDK dependency is missing: guiddef.h' "$tmp/log"

echo 'PASS: libc++ SDK cache invalidation, stable hits and corrupt/missing headers'
