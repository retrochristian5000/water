#!/bin/sh
# Update Water's tracked branch before any build toolchain/bootstrap work.
# Keep dependency gitlinks pinned; 'build.sh update-deps' owns pin changes.
set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mode=${WHP_GIT_UPDATE:-auto}

case "$mode" in
    auto|0|1) ;;
    *) printf 'error: WHP_GIT_UPDATE must be auto, 0, or 1\n' >&2; exit 1 ;;
esac
[ "$mode" != 0 ] || exit 0

warn_or_fail()
{
    if [ "$mode" = 1 ]; then
        printf 'error: %s\n' "$1" >&2
        exit 1
    fi
    printf 'warning: %s; building from the current Water checkout\n' "$1" >&2
}

# Automated CI and local offline/source-archive builds must remain possible.
if [ "$mode" = auto ] && [ -n "${CI:-}" ]; then
    printf 'WHP source update: skipped in CI\n' >&2
    exit 0
fi
if ! command -v git >/dev/null 2>&1; then
    warn_or_fail 'Git is unavailable for Water source refresh'
    exit 0
fi

root=$(git -C "$SOURCE_DIR" rev-parse --show-toplevel 2>/dev/null || true)
if [ -z "$root" ] || [ "$root" != "$SOURCE_DIR" ]; then
    warn_or_fail 'Water sources are not a Git checkout root'
    exit 0
fi
branch=$(git -C "$SOURCE_DIR" symbolic-ref --quiet --short HEAD 2>/dev/null || true)
upstream=$(git -C "$SOURCE_DIR" rev-parse --symbolic-full-name '@{upstream}' 2>/dev/null || true)
remote=
merge_ref=
if [ -n "$branch" ]; then
    remote=$(git -C "$SOURCE_DIR" config --get "branch.$branch.remote" 2>/dev/null || true)
    merge_ref=$(git -C "$SOURCE_DIR" config --get "branch.$branch.merge" 2>/dev/null || true)
fi

# Never overwrite user edits or staged changes; ignore untracked build outputs
# and tracked submodule dirt, which must be handled by the pin initializer.
dirty=0
git -C "$SOURCE_DIR" diff --quiet --ignore-submodules=all -- || dirty=1
git -C "$SOURCE_DIR" diff --cached --quiet --ignore-submodules=all -- || dirty=1
if [ "$dirty" = 1 ]; then
    warn_or_fail 'tracked Water changes prevent a safe fast-forward'
    exit 0
fi
if [ -z "$branch" ]; then
    warn_or_fail 'Water is on a detached HEAD, not an updating branch'
    exit 0
fi
if [ -z "$upstream" ] || [ -z "$remote" ] || [ -z "$merge_ref" ]; then
    warn_or_fail "Water branch '$branch' has no configured upstream"
    exit 0
fi

printf 'WHP source update: %s <- %s\n' "$branch" "$upstream" >&2
# Probe just the selected upstream ref. On a no-change build, avoid fetching
# objects or scanning unrelated branches, tags and submodules.
if [ "$remote" = "." ]; then
    remote_oid=$(git -C "$SOURCE_DIR" rev-parse "$merge_ref^{commit}" 2>/dev/null || true)
else
    remote_line=$(git -C "$SOURCE_DIR" ls-remote "$remote" "$merge_ref" 2>/dev/null || true)
    remote_oid=$(printf '%s\n' "$remote_line" | sed -n '1{s/[[:space:]].*//;p;}')
fi
if [ -z "$remote_oid" ]; then
    warn_or_fail "cannot reach configured Water upstream '$remote/$merge_ref'"
    exit 0
fi

cached_oid=$(git -C "$SOURCE_DIR" rev-parse "$upstream^{commit}" 2>/dev/null || true)
if [ "$remote_oid" != "$cached_oid" ]; then
    if [ "$remote" != "." ]; then
        printf 'WHP source update: fetching changed Water branch (no tags or submodules)\n' >&2
        if ! git -C "$SOURCE_DIR" fetch --no-tags --no-recurse-submodules \
            "$remote" "$merge_ref:$upstream"; then
            warn_or_fail "fetch from configured Water remote '$remote' failed"
            exit 0
        fi
    fi
else
    printf 'WHP source update: upstream unchanged; fetch skipped\n' >&2
fi
if ! git -C "$SOURCE_DIR" merge --ff-only "$upstream"; then
    warn_or_fail 'Water upstream cannot fast-forward this checkout'
    exit 0
fi
printf 'WHP source update: Water current (submodule pins unchanged)\n' >&2
