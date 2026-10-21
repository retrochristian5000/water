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

# One GNU M4 selector for both explicit overrides and macOS gm4 installs.
m4=$(/bin/sh "$root/scripts/whp-build/m4.sh" --print-path) ||
    fail 'could not select a compatible GNU M4'

perl=${PERL:-perl}
aclocal=${ACLOCAL:-aclocal}
automake=${AUTOMAKE:-automake}
make=${MAKE:-make}
for tool in "$perl" "$aclocal" "$automake" "$make" git cksum awk; do
    command -v "$tool" >/dev/null 2>&1 || fail "missing stage-0 host bootstrap tool: $tool"
done

# Autoconf's git bootstrap uses existing Automake executables.  The
# WATER_AUTOMAKE_PROVIDER selection controls Bash macro provenance; it does
# not turn the pinned Automake source into a usable stage-0 executable.
# Fail before staging/building when the two host executables are incompatible.
aclocal_version=$("$aclocal" --version 2>/dev/null) ||
    fail "stage-0 aclocal does not run: $aclocal"
automake_version=$("$automake" --version 2>/dev/null) ||
    fail "stage-0 automake does not run: $automake"
aclocal_version=$(printf '%s\n' "$aclocal_version" | sed -n '1p')
automake_version=$(printf '%s\n' "$automake_version" | sed -n '1p')
case "$aclocal_version:$automake_version" in
    *'GNU automake'*:*'GNU automake'*) ;;
    *) fail "stage-0 tools must be GNU Automake: $aclocal_version / $automake_version" ;;
esac
[ "${aclocal_version##* }" = "${automake_version##* }" ] ||
    fail "stage-0 aclocal and automake versions differ: $aclocal_version / $automake_version"

# A distinct prefix for each toolchain signature prevents stale binary reuse.
identity=$(
    printf '%s\n' 'RECIPE=5' "PIN=$pin" "M4=$m4" \
        "PERL=$(command -v "$perl")" "ACLOCAL=$(command -v "$aclocal")" \
        "AUTOMAKE=$(command -v "$automake")" "MAKE=$(command -v "$make")"
    "$m4" --version | sed -n '1p'
    "$perl" --version | sed -n '2p'
    printf '%s\n' "$aclocal_version" "$automake_version"
    "$make" --version | sed -n '1p'
)
fingerprint=$(printf '%s\n' "$identity" | cksum | awk '{print $1 "-" $2}')
mkdir -p "$build"
build=$(cd "$build" && pwd)
cache=$build/autoconf-bootstrap
prefix=$cache/$pin-$fingerprint
mkdir -p "$cache"
autoconf_usable()
{
    [ -f "$prefix/.whp-ready" ] && [ -x "$prefix/bin/autoconf" ] || return 1
    "$prefix/bin/autoconf" --version >/dev/null 2>&1 || return 1
    # A version banner does not load the installed Perl/M4 modules.
    # Run the actual generator in an isolated directory before trusting cache.
    probe=$(mktemp -d "$cache/.smoke.XXXXXXXX") || return 1
    printf '%s\n' 'AC_INIT([whp-water-bootstrap],[1])' 'AC_OUTPUT' > "$probe/configure.ac"
    (
        cd "$probe" || exit 1
        unset AUTOCONF AUTOM4TE AUTOHEADER AUTORECONF M4
        "$prefix/bin/autoconf" -o configure configure.ac >/dev/null 2>&1
    ) && [ -s "$probe/configure" ]
    result=$?
    rm -rf "$probe"
    return "$result"
}
# A healthy cache is a fast path; interrupted or corrupt caches can be rebuilt.
if autoconf_usable; then
    printf '%s\n' "$prefix/bin/autoconf"
    exit 0
fi
lock=$prefix.lock
mkdir "$lock" 2>/dev/null ||
    fail "another Autoconf bootstrap is already using $prefix"
# Recheck after acquiring the lock: another worker may have just completed.
if autoconf_usable; then
    rmdir "$lock" || fail "cannot release bootstrap lock: $lock"
    printf '%s\n' "$prefix/bin/autoconf"
    exit 0
fi
if [ -e "$prefix" ] || [ -L "$prefix" ]; then
    printf 'WHP Autoconf: clearing incomplete cache %s\n' "$prefix" >&2
    rm -rf "$prefix" || {
        rmdir "$lock" 2>/dev/null || :
        fail "cannot clean incomplete Autoconf cache: $prefix"
    }
fi
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
    M4="$m4"
    PERL="$perl"
    ACLOCAL="$aclocal"
    AUTOMAKE="$automake"
    PATH="$(dirname "$automake"):$PATH"
    export M4 PERL ACLOCAL AUTOMAKE PATH
    /bin/sh ./bootstrap || exit 1
    ./configure --prefix="$prefix" M4="$m4" PERL="$perl" || exit 1
    # These targets match the pinned fork's Automake-generated Makefile.
    # Avoid the default all/install targets, which also build manuals.
    "$make" \
        bin/autoconf bin/autoheader bin/autom4te bin/autoreconf \
        bin/autoscan bin/autoupdate bin/ifnames \
        lib/autom4te.cfg lib/autoconf/autoconf.m4f \
        lib/autoscan/autoscan.list \
        lib/m4sugar/m4sugar.m4f lib/m4sugar/m4sh.m4f \
        lib/autotest/autotest.m4f || exit 1
    "$make" \
        install-binSCRIPTS install-dist_perllibDATA \
        install-nodist_pkgdataDATA install-dist_autoconflibDATA \
        install-nodist_autoconflibDATA install-nodist_autoscanlibDATA \
        install-dist_m4sugarlibDATA install-nodist_m4sugarlibDATA \
        install-dist_autotestlibDATA install-nodist_autotestlibDATA \
        install-dist_buildauxDATA install-data-hook \
        pkgdata_DATA= || exit 1
) >&2 || fail "bootstrap failed for pinned Autoconf $pin"
[ -x "$prefix/bin/autoconf" ] &&
    "$prefix/bin/autoconf" --version >/dev/null 2>&1 ||
    fail "installed fork Autoconf is unusable: $prefix"
: > "$prefix/.whp-ready"
autoconf_usable || fail "installed fork Autoconf cannot generate a configure script: $prefix"
printf '%s\n' "$prefix/bin/autoconf"
