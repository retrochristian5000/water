#!/bin/sh
# Bootstrap the fork at Water's pinned gitlink, not a moving branch tip.
# Keep all generated files out of the tracked source and submodule.
set -eu
[ "$#" -eq 3 ] || { echo 'usage: ensure-autoconf.sh SOURCE_DIR BUILD_DIR SUBMODULES' >&2; exit 2; }
root=$1
build=$2
source=$root/toolchains/autoconf
submodules=$3
fail() { printf 'WHP Autoconf: %s\n' "$*" >&2; exit 1; }
pin=$(git -C "$root" ls-tree HEAD -- toolchains/autoconf 2>/dev/null |
    awk '$2 == "commit" { print $3; exit }')
[ -n "$pin" ] || fail 'missing toolchains/autoconf gitlink'
if [ "$submodules" = 1 ]; then
    git -C "$root" submodule sync -- toolchains/autoconf ||
        fail 'cannot sync Autoconf submodule'
    git -C "$root" submodule update --init --depth 1 -- toolchains/autoconf ||
        git -C "$root" submodule update --init -- toolchains/autoconf ||
        fail 'cannot initialize Autoconf submodule'
fi
git -C "$source" cat-file -e "$pin^{commit}" 2>/dev/null ||
    fail 'pinned Autoconf unavailable; initialize toolchains/autoconf (WHP_SUBMODULES=1)'

# macOS system m4 lacks GNU M4 semantics. Prefer gm4 if available.
if [ -n "${M4:-}" ]; then
    m4=$M4
else
    m4=
    for candidate in gm4 m4; do
        if command -v "$candidate" >/dev/null 2>&1 &&
           "$candidate" --gnu --version 2>/dev/null | grep 'GNU M4' >/dev/null; then
            m4=$(command -v "$candidate")
            break
        fi
    done
fi
[ -n "$m4" ] || fail 'GNU M4 is required (install gm4 or set M4)'
"$m4" --gnu --version 2>/dev/null | grep 'GNU M4' >/dev/null ||
    fail "M4=$m4 is not GNU M4 with --gnu support"

perl=${PERL:-perl}
aclocal=${ACLOCAL:-aclocal}
automake=${AUTOMAKE:-automake}
make=${MAKE:-make}
for tool in "$perl" "$aclocal" "$automake" "$make" git cksum awk; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing host bootstrap tool: $tool"
done

# A distinct prefix for each toolchain signature prevents stale binary reuse.
identity=$(
    printf '%s\n' 'RECIPE=1' "PIN=$pin" "M4=$m4" \
        "PERL=$(command -v "$perl")" "ACLOCAL=$(command -v "$aclocal")" \
        "AUTOMAKE=$(command -v "$automake")" "MAKE=$(command -v "$make")"
    "$m4" --version | sed -n '1p'
    "$perl" --version | sed -n '2p'
    "$aclocal" --version | sed -n '1p'
    "$automake" --version | sed -n '1p'
    "$make" --version | sed -n '1p'
)
fingerprint=$(printf '%s\n' "$identity" | cksum | awk '{print $1 "-" $2}')
mkdir -p "$build"
build=$(cd "$build" && pwd)
cache=$build/autoconf-bootstrap
prefix=$cache/$pin-$fingerprint
if [ -f "$prefix/.whp-ready" ] && [ -x "$prefix/bin/autoconf" ]; then
    "$prefix/bin/autoconf" --version >/dev/null 2>&1 ||
        fail "cached pinned Autoconf is unusable: $prefix"
    printf '%s\n' "$prefix/bin/autoconf"
    exit 0
fi
[ ! -e "$prefix" ] || fail "incomplete Autoconf cache exists: $prefix"
mkdir -p "$cache"
lock=$prefix.lock
mkdir "$lock" 2>/dev/null ||
    fail "another Autoconf bootstrap is already using $prefix"
tmp=$(mktemp -d "$cache/.source.XXXXXXXX") ||
    { rmdir "$lock" 2>/dev/null || :; fail 'cannot create bootstrap directory'; }
cleanup()
{
    rm -rf "$tmp"
    if [ ! -f "$prefix/.whp-ready" ]; then
        rm -rf "$prefix"
    fi
    rmdir "$lock" 2>/dev/null || :
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

# Local shared clone preserves tags needed by git-version-gen, without
# writing any generated files into toolchains/autoconf itself.
git clone -q --shared "$source" "$tmp/source" || fail 'cannot stage Autoconf source'
git -C "$tmp/source" checkout -q --detach "$pin" || fail 'cannot check out pinned Autoconf'
(
    cd "$tmp/source"
    M4="$m4" PERL="$perl" ACLOCAL="$aclocal" AUTOMAKE="$automake" ./bootstrap || exit 1
    ./configure --prefix="$prefix" M4="$m4" PERL="$perl" || exit 1
    "$make" || exit 1
    "$make" install || exit 1
) >&2 || fail "bootstrap failed for pinned Autoconf $pin"
[ -x "$prefix/bin/autoconf" ] && "$prefix/bin/autoconf" --version >/dev/null 2>&1 ||
    fail "installed fork Autoconf is unusable: $prefix"
: > "$prefix/.whp-ready"
printf '%s\n' "$prefix/bin/autoconf"
