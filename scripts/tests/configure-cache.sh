#!/bin/sh
# Exercise cache integrity without building Autoconf or touching tracked files.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-configure-cache.XXXXXXXX")
trap 'rm -rf "$tmp"' 0
trap 'exit 1' 1 2 3 15
command -v git >/dev/null 2>&1 || { echo 'SKIP: git unavailable'; exit 0; }
export GIT_ALLOW_PROTOCOL=file GIT_TERMINAL_PROMPT=0
mkdir -p "$tmp/upstream" "$tmp/water/build" "$tmp/water/scripts/whp-build"
git init -q -b master "$tmp/upstream"
git -C "$tmp/upstream" config user.name 'Water test'
git -C "$tmp/upstream" config user.email 'test@example.invalid'
echo pinned > "$tmp/upstream/README"
git -C "$tmp/upstream" add README
git -C "$tmp/upstream" commit -qm 'fixture Autoconf revision'

git init -q -b master "$tmp/water"
git -C "$tmp/water" config user.name 'Water test'
git -C "$tmp/water" config user.email 'test@example.invalid'
git -C "$tmp/water" submodule add -q "$tmp/upstream" toolchains/autoconf
for file in configure.ac aclocal.m4 VERSION configure \
            scripts/whp-build/ensure-autoconf.sh; do
    printf '%s\n' "$file" > "$tmp/water/$file"
done
git -C "$tmp/water" add -A
git -C "$tmp/water" commit -qm 'fixture Water revision'

cache=$root/scripts/whp-build/configure-cache.sh
build=$tmp/water/build
: > "$build/.whp-autoconf-state"
if /bin/sh "$cache" check "$tmp/water" "$build"; then
    echo 'FAIL: unrecorded configure accepted' >&2; exit 1
fi
/bin/sh "$cache" record "$tmp/water" "$build"
/bin/sh "$cache" check "$tmp/water" "$build"

for file in configure.ac aclocal.m4 VERSION configure \
            scripts/whp-build/ensure-autoconf.sh; do
    cp "$tmp/water/$file" "$tmp/saved"
    echo changed >> "$tmp/water/$file"
    if /bin/sh "$cache" check "$tmp/water" "$build"; then
        echo "FAIL: modified $file accepted" >&2; exit 1
    fi
    mv "$tmp/saved" "$tmp/water/$file"
    /bin/sh "$cache" check "$tmp/water" "$build"
done

# HEAD gitlink changes must invalidate the manifest independently of files.
echo next >> "$tmp/water/toolchains/autoconf/README"
git -C "$tmp/water/toolchains/autoconf" add README
git -C "$tmp/water/toolchains/autoconf" -c user.name='Water test' \
    -c user.email='test@example.invalid' commit -qm 'advance pin'
git -C "$tmp/water" add toolchains/autoconf
git -C "$tmp/water" commit -qm 'change Autoconf pin'
if /bin/sh "$cache" check "$tmp/water" "$build"; then
    echo 'FAIL: updated Autoconf pin accepted' >&2; exit 1
fi
/bin/sh "$cache" record "$tmp/water" "$build"
/bin/sh "$cache" check "$tmp/water" "$build"
rm "$build/.whp-autoconf-state"
if /bin/sh "$cache" check "$tmp/water" "$build"; then
    echo 'FAIL: missing generator state accepted' >&2; exit 1
fi
echo 'PASS: generated configure provenance, mutation and pin invalidation'
