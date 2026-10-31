#!/bin/sh
# Bootstrap Water's pinned Automake executables using seed Autoconf.
# Called only when generated configure needs rebuilding; no tracked source writes.
set -eu
[ "$#" -eq 3 ] || {
    echo 'usage: ensure-automake.sh SOURCE_DIR BUILD_DIR SUBMODULES' >&2
    exit 2
}
root=$1
build=$2
submodules=$3
source=$root/toolchains/automake
fail() { printf 'WHP Automake: %s\n' "$*" >&2; exit 1; }

pin=$(git -C "$root" ls-tree HEAD -- toolchains/automake 2>/dev/null |
    awk '$2 == "commit" { print $3; exit }')
[ -n "$pin" ] || fail 'missing toolchains/automake gitlink'
if [ "$submodules" = 1 ]; then
    git -C "$root" submodule sync -- toolchains/automake ||
        fail 'cannot sync Automake submodule'
    git -C "$root" submodule update --init --depth 1 -- toolchains/automake ||
        git -C "$root" submodule update --init -- toolchains/automake ||
        fail 'cannot initialize pinned Automake submodule'
fi
git -C "$source" cat-file -e "$pin^{commit}" 2>/dev/null ||
    fail 'pinned Automake unavailable; initialize toolchains/automake (WHP_SUBMODULES=1)'

# Automake self-hosts aclocal/automake but needs seed Autoconf/autom4te.
seed_autoconf=${WHP_AUTOMAKE_SEED_AUTOCONF:-autoconf}
seed_autom4te=${WHP_AUTOMAKE_SEED_AUTOM4TE:-autom4te}
perl=$(/bin/sh "$root/perl.sh" --print-path) ||
    fail 'native Perl validation failed (set PERL to a working interpreter)'
# Generated script shebangs cannot safely contain a Perl path with spaces.
case "$perl" in *[[:space:]]*) fail "pinned Automake bootstrap cannot use a Perl path with whitespace: $perl" ;; esac
make=${MAKE:-}
if [ -z "$make" ]; then
    make=$(command -v gmake 2>/dev/null || command -v make 2>/dev/null || true)
fi
for tool in "$seed_autoconf" "$seed_autom4te" "$perl" "$make" git cksum awk sed grep; do
    [ -n "$tool" ] && command -v "$tool" >/dev/null 2>&1 ||
        fail "missing seed bootstrap tool: $tool"
done
seed_autoconf=$(command -v "$seed_autoconf")
seed_autom4te=$(command -v "$seed_autom4te")
make=$(command -v "$make")
ac_version=$("$seed_autoconf" --version 2>/dev/null | sed -n '1p') ||
    fail 'seed Autoconf version probe failed'
am4_version=$("$seed_autom4te" --version 2>/dev/null | sed -n '1p') ||
    fail 'seed autom4te version probe failed'
case "$ac_version:$am4_version" in
    *'GNU Autoconf'*:*'GNU Autoconf'*) ;;
    *) fail "seed Autoconf/autom4te are not GNU Autoconf tools: $ac_version / $am4_version" ;;
esac
[ "${ac_version##* }" = "${am4_version##* }" ] ||
    fail "seed Autoconf/autom4te versions differ: $ac_version / $am4_version"
"$make" --version 2>/dev/null | grep 'GNU Make' >/dev/null ||
    fail 'GNU Make is required for the pinned Automake bootstrap'
m4=$(/bin/sh "$root/scripts/whp-build/m4.sh" --print-path) ||
    fail 'could not select GNU M4'

# A Git checkout regenerates the Automake Texinfo manual during make.
# Homebrew's texinfo formula may be keg-only on macOS; locate its
# makeinfo without changing the user's shell or installing packages.
makeinfo=$(command -v makeinfo 2>/dev/null || true)
if [ -z "$makeinfo" ] && command -v brew >/dev/null 2>&1; then
    texinfo_prefix=$(brew --prefix texinfo 2>/dev/null || true)
    if [ -n "$texinfo_prefix" ] && [ -x "$texinfo_prefix/bin/makeinfo" ]; then
        makeinfo=$texinfo_prefix/bin/makeinfo
    fi
fi
[ -n "$makeinfo" ] || fail 'GNU Texinfo makeinfo required (macOS: brew install texinfo)'
case "$makeinfo" in
    /*) ;;
    *) fail "makeinfo must resolve to an absolute executable: $makeinfo" ;;
esac
makeinfo_version=$("$makeinfo" --version 2>/dev/null | sed -n '1p') ||
    fail 'makeinfo version probe failed'
case "$makeinfo_version" in
    *'GNU texinfo'*|*'GNU Texinfo'*) ;;
    *) fail "makeinfo is not GNU Texinfo: $makeinfo_version" ;;
esac
PATH="$(dirname "$makeinfo"):$PATH"
export PATH

# Distinct tool fingerprints prevent stale install reuse.
identity=$(
    printf '%s\n' 'RECIPE=2' "PIN=$pin" "AC=$seed_autoconf:$ac_version" \
        "AUTOM4TE=$seed_autom4te:$am4_version" "PERL=$perl" "MAKE=$make" "M4=$m4" \
        "MAKEINFO=$makeinfo:$makeinfo_version"
    "$perl" --version | sed -n '2p'
    "$make" --version | sed -n '1p'
    "$m4" --version | sed -n '1p'
    cksum < "$0"
)
fingerprint=$(printf '%s\n' "$identity" | cksum | awk '{print $1 "-" $2}')
mkdir -p "$build"
build=$(cd "$build" && pwd)
cache=$build/automake-bootstrap
prefix=$cache/$pin-$fingerprint
usable()
{
    [ -f "$prefix/.whp-ready" ] &&
    [ -x "$prefix/bin/automake" ] && [ -x "$prefix/bin/aclocal" ] &&
    "$prefix/bin/automake" --version 2>/dev/null | grep 'GNU automake' >/dev/null &&
    "$prefix/bin/aclocal" --version 2>/dev/null | grep 'GNU automake' >/dev/null &&
    "$prefix/bin/aclocal" --print-ac-dir >/dev/null 2>&1
}
if usable; then
    printf '%s\n' "$prefix"
    exit 0
fi
mkdir -p "$cache"
lock=$prefix.lock
mkdir "$lock" 2>/dev/null ||
    fail "another Automake bootstrap is using $prefix"
if usable; then
    rmdir "$lock" || fail "cannot release bootstrap lock: $lock"
    printf '%s\n' "$prefix"
    exit 0
fi
if [ -e "$prefix" ] || [ -L "$prefix" ]; then
    printf 'WHP Automake: clearing incomplete cache %s\n' "$prefix" >&2
    rm -rf "$prefix" || {
        rmdir "$lock" 2>/dev/null || :
        fail 'cannot clean incomplete Automake cache'
    }
fi
tmp=$(mktemp -d "$cache/.source.XXXXXXXX") || {
    rmdir "$lock" 2>/dev/null || :
    fail 'cannot create bootstrap directory'
}
cleanup()
{
    rm -rf "$tmp"
    if [ ! -f "$prefix/.whp-ready" ]; then rm -rf "$prefix"; fi
    rmdir "$lock" 2>/dev/null || :
}
trap cleanup 0
trap 'exit 1' 1 2 3 15

git clone -q --shared "$source" "$tmp/source" || fail 'cannot stage Automake source'
git -C "$tmp/source" checkout -q --detach "$pin" || fail 'cannot check out pinned Automake'
mkdir -p "$tmp/build" "$tmp/seed-bin"
ln -s "$m4" "$tmp/seed-bin/m4" || fail 'cannot expose validated GNU M4'
(
    cd "$tmp/source"
    PATH="$tmp/seed-bin:$PATH" M4="$m4" PERL="$perl" \
        AUTOCONF="$seed_autoconf" AUTOM4TE="$seed_autom4te" /bin/sh ./bootstrap
) >&2 || fail "pinned Automake bootstrap failed at $pin"
[ -f "$tmp/source/configure" ] || fail 'Automake bootstrap did not generate configure'
(
    cd "$tmp/build"
    PATH="$tmp/seed-bin:$PATH"
    M4="$m4"
    PERL="$perl"
    AUTOCONF="$seed_autoconf"
    AUTOM4TE="$seed_autom4te"
    export PATH M4 PERL AUTOCONF AUTOM4TE
    /bin/sh "$tmp/source/configure" --prefix="$prefix" || exit 1
    "$make" || exit 1
    "$make" install || exit 1
) >&2 || fail "could not build/install pinned Automake $pin"
[ -x "$prefix/bin/automake" ] && [ -x "$prefix/bin/aclocal" ] ||
    fail 'pinned Automake did not install both executables'
"$prefix/bin/automake" --version 2>/dev/null | grep 'GNU automake' >/dev/null &&
"$prefix/bin/aclocal" --version 2>/dev/null | grep 'GNU automake' >/dev/null &&
"$prefix/bin/aclocal" --print-ac-dir >/dev/null 2>&1 ||
    fail 'installed Automake/aclocal pair is unusable'
: > "$prefix/.whp-ready"
printf '%s\n' "$prefix"
