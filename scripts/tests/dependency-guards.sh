#!/bin/sh
# Exercise Water's real update-deps function without invoking its bootstrap.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-dep-guards.XXXXXXXX")
trap 'rm -rf "$tmp"' 0
trap 'exit 1' 1 2 3 15
export GIT_ALLOW_PROTOCOL=file GIT_TERMINAL_PROMPT=0
command -v git >/dev/null 2>&1 || { echo 'SKIP: git unavailable'; exit 0; }
die() { printf '%s\n' "$*" >&2; exit 1; }

# Import the function from build.sh, leaving the rest of Water untouched.
awk '/^update_dependency_pins\(\)$/,/^}$/' "$root/build.sh" > "$tmp/function.sh"
grep -q '^update_dependency_pins()$' "$tmp/function.sh"
. "$tmp/function.sh"

for module in musl other; do
    git init -q -b master "$tmp/$module"
    git -C "$tmp/$module" config user.name 'Water test'
    git -C "$tmp/$module" config user.email 'test@example.invalid'
    printf 'v1\n' > "$tmp/$module/content.txt"
    git -C "$tmp/$module" add content.txt
    git -C "$tmp/$module" commit -qm initial
done
SOURCE_DIR=$tmp/water
git init -q -b master "$SOURCE_DIR"
git -C "$SOURCE_DIR" config user.name 'Water test'
git -C "$SOURCE_DIR" config user.email 'test@example.invalid'
printf 'tracked\n' > "$SOURCE_DIR/README.md"
git -C "$SOURCE_DIR" submodule add -q "$tmp/musl" libs/musl/upstream
git -C "$SOURCE_DIR" submodule add -q "$tmp/other" toolchains/new-tool
mkdir -p "$SOURCE_DIR/toolchains/not-a-module"
printf 'ordinary\n' > "$SOURCE_DIR/toolchains/not-a-module/README"
git -C "$SOURCE_DIR" add -A
git -C "$SOURCE_DIR" commit -qm 'initial source'

# Neither a regular tracked file nor an unknown path is a valid submodule.
if (update_dependency_pins toolchains/not-a-module/README >/dev/null 2>&1); then
    echo 'FAIL: ordinary file accepted' >&2; exit 1
fi
if (update_dependency_pins toolchains/nonexistent >/dev/null 2>&1); then
    echo 'FAIL: unknown submodule accepted' >&2; exit 1
fi

# Dirty tracked changes remain protected.
printf 'dirty\n' >> "$SOURCE_DIR/README.md"
if (update_dependency_pins libs/musl/upstream >/dev/null 2>&1); then
    echo 'FAIL: dirty source accepted' >&2; exit 1
fi
git -C "$SOURCE_DIR" checkout -q -- README.md

# musl is absent from the old allowlist; the actual gitlink must now advance.
printf 'v2\n' > "$tmp/musl/content.txt"
git -C "$tmp/musl" add content.txt
git -C "$tmp/musl" commit -qm update
update_dependency_pins libs/musl/upstream >/dev/null
new=$(git -C "$tmp/musl" rev-parse HEAD)
staged=$(git -C "$SOURCE_DIR" ls-files -s libs/musl/upstream | awk '{print $2}')
old=$(git -C "$SOURCE_DIR" ls-tree HEAD -- libs/musl/upstream | awk '{print $3}')
[ "$new" = "$staged" ] && [ "$old" != "$staged" ] ||
    { echo 'FAIL: musl gitlink was not staged safely' >&2; exit 1; }

# A future named fork works without changing the updater again.
git -C "$SOURCE_DIR" reset -q --hard HEAD
git -C "$SOURCE_DIR" submodule update --init --checkout -- libs/musl/upstream >/dev/null
update_dependency_pins toolchains/new-tool >/dev/null
echo 'PASS: metadata-backed update-deps, musl, future modules, dirty guards'
