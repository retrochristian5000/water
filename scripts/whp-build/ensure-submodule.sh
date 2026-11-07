#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Initialize selected WATER gitlinks without rewinding newer local checkouts.
set -eu

[ "$#" -ge 2 ] || {
    printf 'usage: ensure-submodule.sh WATER_ROOT PATH...\n' >&2
    exit 2
}
root=$1
shift
fail()
{
    printf 'error: WATER submodule: %s\n' "$*" >&2
    exit 1
}
git()
{
    if [ -n "${WHP_GIT_SEED:-}" ]; then
        "$WHP_GIT_SEED" "$@"
    else
        command git "$@"
    fi
}
[ -d "$root" ] || fail "missing source root: $root"
for path do
    case "$path" in
        ''|/*|../*|*/../*|*/..|./*|*/./*|*/.) fail "unsafe path: $path" ;;
    esac
    entry=$(git -C "$root" ls-tree HEAD -- "$path") ||
        fail "cannot read gitlink: $path"
    pin=$(printf '%s\n' "$entry" | awk -F '\t' -v p="$path" '
        $2 == p && $1 ~ /^160000 commit [0-9a-f]+$/ {
            split($1, f, " "); print f[3]; exit
        }')
    [ -n "$pin" ] || fail "not a pinned WATER dependency: $path"
    target=$root/$path
    [ ! -L "$target" ] || fail "refusing symlinked source: $path"
    if [ ! -e "$target/.git" ]; then
        [ "${WHP_SUBMODULES:-1}" = 1 ] ||
            fail "uninitialized $path (WHP_SUBMODULES=0)"
        git -C "$root" submodule sync -- "$path" ||
            fail "cannot sync source: $path"
        git -C "$root" submodule update --init --recursive --depth 1 -- "$path" ||
            git -C "$root" submodule update --init --recursive -- "$path" ||
            fail "cannot initialize source: $path"
    fi
    top=$(git -C "$target" rev-parse --show-toplevel 2>/dev/null) ||
        fail "not an initialized checkout: $path"
    physical_target=$(CDPATH= cd -P -- "$target" && pwd -P) ||
        fail "cannot resolve checkout: $path"
    physical_top=$(CDPATH= cd -P -- "$top" && pwd -P) ||
        fail "cannot resolve checkout root: $path"
    [ "$physical_target" = "$physical_top" ] ||
        fail "unexpected nested Git worktree: $path"
    current=$(git -C "$target" rev-parse --verify HEAD) ||
        fail "cannot read HEAD: $path"
    if [ "$current" = "$pin" ]; then
        printf 'WATER submodule %s: pinned %s\n' "$path" "$pin" >&2
        continue
    fi
    if ! git -C "$target" cat-file -e "$pin^{commit}" 2>/dev/null; then
        [ "${WHP_SUBMODULES:-1}" = 1 ] ||
            fail "pinned commit unavailable locally for $path: $pin"
        printf 'WATER submodule %s: fetching missing pin object (without checkout)\n' "$path" >&2
        git -C "$target" fetch --no-tags --no-recurse-submodules origin "$pin" ||
            fail "cannot fetch pinned history for $path: $pin (checkout preserved)"
    fi
    if git -C "$target" merge-base --is-ancestor "$pin" "$current"; then
        printf 'WATER submodule %s: keeping newer checkout %s (pin %s)\n' \
            "$path" "$current" "$pin" >&2
        continue
    else
        rc=$?
        [ "$rc" -eq 1 ] || fail "cannot compare pinned ancestry for $path"
    fi
    if git -C "$target" merge-base --is-ancestor "$current" "$pin"; then
        attached=$(git -C "$target" symbolic-ref --quiet --short HEAD 2>/dev/null || true)
        dirty=$(git -C "$target" status --porcelain --untracked-files=all) ||
            fail "cannot inspect local changes: $path"
        if [ -n "$attached" ] || [ -n "$dirty" ]; then
            printf 'warning: WATER submodule %s: preserving older development checkout (branch or local files)\n' \
                "$path" >&2
            continue
        fi
        git -C "$root" submodule update --init --no-fetch -- "$path" ||
            fail "cannot fast-forward detached submodule: $path"
        [ "$(git -C "$target" rev-parse HEAD)" = "$pin" ] ||
            fail "submodule failed to reach pin: $path"
        printf 'WATER submodule %s: fast-forwarded to pin %s\n' "$path" "$pin" >&2
        continue
    else
        rc=$?
        [ "$rc" -eq 1 ] || fail "cannot compare local ancestry for $path"
    fi
    printf 'warning: WATER submodule %s: preserving divergent/incomplete history (local %s; pin %s)\n' \
        "$path" "$current" "$pin" >&2
done
