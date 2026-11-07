#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Native stage-0 Autoconf bridge for WATER's pinned Automake bootstrap.
# This bridge selects an existing host generator; it never bootstraps itself.
set -eu

case "${1:-}" in
    --whp-autoconf-adapter-probe)
        printf '%s\n' whp-water-autoconf-adapter
        exit 0 ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
error()
{
    printf 'WHP autoconf: %s\n' "$*" >&2
}

resolve_candidate()
(
    requested=$1
    [ -n "$requested" ] || exit 1
    case "$requested" in
        *[[:space:]]*) exit 1 ;;
        */*) candidate=$requested ;;
        *) candidate=$(command -v "$requested" 2>/dev/null || true) ;;
    esac
    [ -n "$candidate" ] && [ -x "$candidate" ] || exit 1
    case "$candidate" in
        /*) ;;
        */*)
            directory=$(CDPATH= cd -P -- "${candidate%/*}" && pwd -P) || exit 1
            candidate=$directory/${candidate##*/}
            ;;
        *) exit 1 ;;
    esac
    marker=$("$candidate" --whp-autoconf-adapter-probe 2>/dev/null || true)
    [ "$marker" != whp-water-autoconf-adapter ] || exit 1

    banner=$("$candidate" --version 2>/dev/null) || exit 1
    case "$banner" in
        *'GNU Autoconf'*) ;;
        *) exit 1 ;;
    esac

    # An Autoconf version banner is insufficient: verify generated output.
    scratch=$(umask 077; mktemp -d "${TMPDIR:-/tmp}/water-autoconf.XXXXXXXX") || exit 1
    trap 'rm -rf -- "$scratch"' 0
    trap 'exit 1' 1 2 3 15
    printf 'AC_INIT([whp-water-autoconf],[1])\nAC_OUTPUT\n' > "$scratch/configure.ac"
    m4=$(/bin/sh "$root/scripts/whp-build/m4.sh" --print-path) || exit 1
    # Run inside the probe directory: Autoconf may create autom4te.cache.
    # Never leave generated cache files in WATER's tracked source tree.
    (
        cd "$scratch" || exit 1
        M4=$m4 "$candidate" --output=configure configure.ac >/dev/null 2>&1
    ) || exit 1
    [ -s "$scratch/configure" ] || exit 1
    grep -Fq 'whp-water-autoconf' "$scratch/configure" || exit 1
    printf '%s\n' "$candidate"
)

select_seed()
{
    # Explicit choices are authoritative; never silently replace a broken one.
    if [ -n "${WHP_AUTOMAKE_SEED_AUTOCONF:-}" ]; then
        resolve_candidate "$WHP_AUTOMAKE_SEED_AUTOCONF" || {
            error "WHP_AUTOMAKE_SEED_AUTOCONF is not a functional GNU Autoconf: $WHP_AUTOMAKE_SEED_AUTOCONF"
            return 1
        }
        return 0
    fi
    if [ -n "${AUTOCONF:-}" ]; then
        resolve_candidate "$AUTOCONF" || {
            error "AUTOCONF is not a functional GNU Autoconf: $AUTOCONF"
            return 1
        }
        return 0
    fi

    for name in autoconf /usr/bin/autoconf /opt/homebrew/bin/autoconf /usr/local/bin/autoconf; do
        if resolved=$(resolve_candidate "$name"); then
            printf '%s\n' "$resolved"
            return 0
        fi
    done
    error 'no working host Autoconf found; install a stage-0 Autoconf or set WHP_AUTOMAKE_SEED_AUTOCONF'
    return 1
}

selected=$(select_seed) || exit 1
case "${1:-}" in
    --print-path|--print-seed) printf '%s\n' "$selected"; exit 0 ;;
    --check)
        printf 'WHP Autoconf seed: %s\n' "$selected"
        exit 0 ;;
esac
exec "$selected" "$@"
