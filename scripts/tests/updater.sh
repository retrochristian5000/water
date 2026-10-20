#!/bin/sh
# Safe Git-only Water updater regression; never build or alter the real tree.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
source_script=${WATER_TEST_SOURCE:-"$root/build.sh"}
source_helper=${WATER_TEST_HELPER:-"$root/scripts/update-source.sh"}
[ -f "$source_script" ] && [ -f "$source_helper" ] || {
    printf 'Water updater test requires build.sh and scripts/update-source.sh\n' >&2
    exit 2
}

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-updater-test.XXXXXXXX")
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
export GIT_ALLOW_PROTOCOL=file
export GIT_TERMINAL_PROMPT=0

for module in fluidsynth bash zlib; do
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
mkdir -p "$tmp/source/scripts"
cp "$source_script" "$tmp/source/build.sh"
cp "$source_helper" "$tmp/source/scripts/update-source.sh"
git -C "$tmp/source" submodule add -q "$tmp/fluidsynth" libs/fluidsynth
git -C "$tmp/source" submodule add -q "$tmp/bash" toolchains/bash
git -C "$tmp/source" submodule add -q "$tmp/zlib" libs/zlib/source
git -C "$tmp/source" add -A
git -C "$tmp/source" commit -qm initial

git init -q --bare "$tmp/remote.git"
git -C "$tmp/remote.git" symbolic-ref HEAD refs/heads/master
git -C "$tmp/source" remote add origin "$tmp/remote.git"
git -C "$tmp/source" push -q -u origin master
git clone -q "$tmp/remote.git" "$tmp/work"
git clone -q "$tmp/remote.git" "$tmp/auto"

# Make the remote's build script newer than both worktrees.
printf '\n# new Water build rules\n' >> "$tmp/source/build.sh"
git -C "$tmp/source" add build.sh
git -C "$tmp/source" commit -qm 'new Water revision'
git -C "$tmp/source" push -q

# Normal source refresh re-enters the revised script in the same invocation,
# even for a cheap probe that doesn't proceed into toolchain bootstrap.
CI= WHP_GIT_UPDATE=auto sh "$tmp/auto/build.sh" --help > "$tmp/auto.log" 2>&1 || {
    cat "$tmp/auto.log" >&2
    exit 1
}
grep -Fq 'WHP source update: re-entering refreshed build.sh' "$tmp/auto.log"
[ "$(git -C "$tmp/auto" rev-parse HEAD)" = "$(git -C "$tmp/source" rev-parse HEAD)" ]
[ "$(grep -Fc 're-entering refreshed build.sh' "$tmp/auto.log")" -eq 1 ]

# Explicit update retains the original pinned-submodule semantics.
WHP_AUTOMAKE_SOURCE_DIR="$tmp/no-automake" WHP_LLVM_SOURCE_DIR="$tmp/no-llvm" \
    sh "$tmp/work/build.sh" update > "$tmp/update.log" 2>&1 || {
    cat "$tmp/update.log" >&2
    exit 1
}
grep -Fq 'WHP source update: re-entering refreshed build.sh' "$tmp/update.log"
[ "$(git -C "$tmp/work" rev-parse HEAD)" = "$(git -C "$tmp/source" rev-parse HEAD)" ]
for path in libs/fluidsynth libs/zlib/source toolchains/bash; do
    expected=$(git -C "$tmp/work" ls-tree HEAD -- "$path" | awk '{print $3}')
    [ "$expected" = "$(git -C "$tmp/work/$path" rev-parse HEAD)" ]
done

WHP_AUTOMAKE_SOURCE_DIR="$tmp/no-automake" WHP_LLVM_SOURCE_DIR="$tmp/no-llvm" \
    sh "$tmp/work/build.sh" update > "$tmp/current.log" 2>&1 || {
    cat "$tmp/current.log" >&2
    exit 1
}
grep -Fq 'upstream unchanged; fetch skipped' "$tmp/current.log"
[ "$(grep -Fc 'WHP source update: master <- ' "$tmp/current.log")" -eq 1 ]

# A dirty tracked checkout is left intact in automatic mode and rejected in
# strict mode. Neither path may discard or modify the user's source changes.
printf '\n# local uncommitted work\n' >> "$tmp/auto/build.sh"
CI= WHP_GIT_UPDATE=auto sh "$tmp/auto/build.sh" --help > "$tmp/dirty.log" 2>&1
grep -Fq 'tracked Water changes prevent a safe fast-forward' "$tmp/dirty.log"
if WHP_GIT_UPDATE=1 sh "$tmp/auto/build.sh" --help > "$tmp/strict.log" 2>&1; then
    printf 'strict source update unexpectedly accepted tracked changes\n' >&2
    exit 1
fi
grep -Fq 'tracked Water changes prevent a safe fast-forward' "$tmp/strict.log"

git -C "$tmp/auto" checkout -q -- build.sh
# CI and deliberately disabled source updates must skip even a reachable
# upstream. Auto offline mode warns, rather than preventing local builds.
CI=1 WHP_GIT_UPDATE=auto sh "$tmp/auto/build.sh" --help > "$tmp/ci.log" 2>&1
grep -Fq 'skipped in CI' "$tmp/ci.log"
WHP_GIT_UPDATE=0 sh "$tmp/auto/build.sh" --help > "$tmp/disabled.log" 2>&1
if grep -F 'WHP source update:' "$tmp/disabled.log" >/dev/null; then
    printf 'disabled source updater performed work\n' >&2
    exit 1
fi
git -C "$tmp/auto" remote set-url origin "$tmp/unavailable.git"
CI= WHP_GIT_UPDATE=auto sh "$tmp/auto/build.sh" --help > "$tmp/offline.log" 2>&1
grep -Fq 'cannot reach configured Water upstream' "$tmp/offline.log"
if WHP_GIT_UPDATE=1 sh "$tmp/auto/build.sh" --help > "$tmp/offline-strict.log" 2>&1; then
    printf 'strict update unexpectedly accepted an unreachable remote\n' >&2
    exit 1
fi
grep -Fq 'cannot reach configured Water upstream' "$tmp/offline-strict.log"

# A consuming repository intentionally leaves Water detached at its gitlink.
# Auto mode must identify a matching pin without fetching or reporting damage.
git init -q -b master "$tmp/consumer"
git -C "$tmp/consumer" config user.name 'Water updater test'
git -C "$tmp/consumer" config user.email 'test@example.invalid'
git -C "$tmp/consumer" submodule add -q "$tmp/remote.git" toolchains/wine
git -C "$tmp/consumer" commit -qm 'pin Water'
git clone -q "$tmp/consumer" "$tmp/consumer-work"
git -C "$tmp/consumer-work" submodule update -q --init --checkout -- toolchains/wine
child="$tmp/consumer-work/toolchains/wine"
pinned=$(git -C "$tmp/consumer-work" ls-files -s -- toolchains/wine | awk '$1 == "160000" { print $2; exit }')
[ "$(git -C "$child" rev-parse HEAD)" = "$pinned" ]
[ -z "$(git -C "$child" symbolic-ref --quiet --short HEAD 2>/dev/null || true)" ]
CI= WHP_GIT_UPDATE=auto sh "$child/scripts/update-source.sh" > "$tmp/pinned.log" 2>&1
grep -Fq 'pinned Water submodule (detached HEAD expected); refresh skipped' "$tmp/pinned.log"
[ "$(git -C "$child" rev-parse HEAD)" = "$pinned" ]
if WHP_GIT_UPDATE=1 sh "$child/scripts/update-source.sh" > "$tmp/pinned-strict.log" 2>&1; then
    printf 'strict source update unexpectedly accepted a detached submodule\n' >&2
    exit 1
fi
grep -Fq 'Water is on a detached HEAD' "$tmp/pinned-strict.log"

# A mismatched gitlink is NOT an expected, pinned checkout.
git -C "$child" checkout -q --detach HEAD^
CI= WHP_GIT_UPDATE=auto sh "$child/scripts/update-source.sh" > "$tmp/mismatched.log" 2>&1
grep -Fq 'Water submodule HEAD differs from the consumer gitlink' "$tmp/mismatched.log"
git -C "$tmp/consumer-work" submodule update -q --checkout -- toolchains/wine
[ "$(git -C "$child" rev-parse HEAD)" = "$pinned" ]

# A detached standalone Water checkout remains a warning, not a pin.
git -C "$tmp/auto" checkout -q --detach HEAD
CI= WHP_GIT_UPDATE=auto sh "$tmp/auto/scripts/update-source.sh" > "$tmp/standalone.log" 2>&1
grep -Fq 'Water is on a detached HEAD, not an updating branch' "$tmp/standalone.log"

printf 'PASS: Water auto-refresh, one-shot reentry, pinned submodules, offline and dirty guards\n'
