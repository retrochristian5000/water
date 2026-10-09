#!/bin/sh
# Exercise a Water fast-forward followed by pinned-submodule initialization
# in a disposable local Git fixture; never touch the real checkout.
set -eu

source_script=${WATER_TEST_SOURCE:-$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)/build.sh}
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-updater-test.XXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
export GIT_ALLOW_PROTOCOL=file

for module in fluidsynth bash; do
    git init -q -b master "$tmp/$module"
    git -C "$tmp/$module" config user.name 'Water updater test'
    git -C "$tmp/$module" config user.email 'test@example.invalid'
    printf '%s\n' "$module" > "$tmp/$module/content.txt"
    git -C "$tmp/$module" add content.txt
    git -C "$tmp/$module" commit -qm initial
done

git init -q -b master "$tmp/source"
git -C "$tmp/source" config user.name 'Water updater test'
git -C "$tmp/source" config user.email 'test@example.invalid'
cp "$source_script" "$tmp/source/build.sh"
git -C "$tmp/source" submodule add -q "$tmp/fluidsynth" libs/fluidsynth
git -C "$tmp/source" submodule add -q "$tmp/bash" toolchains/bash
git -C "$tmp/source" add -A
git -C "$tmp/source" commit -qm initial

git init -q --bare "$tmp/remote.git"
git -C "$tmp/remote.git" symbolic-ref HEAD refs/heads/master
git -C "$tmp/source" remote add origin "$tmp/remote.git"
git -C "$tmp/source" push -q -u origin master
git clone -q "$tmp/remote.git" "$tmp/work"

# Advance Water after cloning so the updater must pull its parent repository.
printf '\n# new Water build rules\n' >> "$tmp/source/build.sh"
git -C "$tmp/source" add build.sh
git -C "$tmp/source" commit -qm 'new Water revision'
git -C "$tmp/source" push -q

WHP_AUTOMAKE_SOURCE_DIR="$tmp/no-automake" WHP_LLVM_SOURCE_DIR="$tmp/no-llvm" \
    sh "$tmp/work/build.sh" update > "$tmp/update.log" 2>&1 || {
    cat "$tmp/update.log" >&2
    exit 1
}

grep -Fq 'WHP source updated; restarting updater' "$tmp/update.log"
[ "$(git -C "$tmp/work" rev-parse HEAD)" = "$(git -C "$tmp/source" rev-parse HEAD)" ]
for path in libs/fluidsynth toolchains/bash; do
    expected=$(git -C "$tmp/work" ls-tree HEAD -- "$path" | awk '{print $3}')
    [ "$expected" = "$(git -C "$tmp/work/$path" rev-parse HEAD)" ]
done

WHP_AUTOMAKE_SOURCE_DIR="$tmp/no-automake" WHP_LLVM_SOURCE_DIR="$tmp/no-llvm" \
    sh "$tmp/work/build.sh" update > "$tmp/current.log" 2>&1 || {
    cat "$tmp/current.log" >&2
    exit 1
}
[ "$(grep -Fc 'WHP source update: git pull' "$tmp/current.log")" -eq 1 ]
printf 'PASS: Water fast-forward and pinned-submodule initialization\n'
