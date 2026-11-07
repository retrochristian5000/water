#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Native stage-0 autom4te bridge for WATER's pinned Automake bootstrap.
# A runnable host Autoconf/autom4te pair is needed before Automake can build.
set -eu

case "${1:-}" in
    --whp-autom4te-adapter-probe)
        printf '%s\n' whp-water-autom4te-adapter
        exit 0 ;;
esac

root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
error()
{
    printf 'WHP autom4te: %s\n' "$*" >&2
}

# Keep this an executable path, not an evaluated shell command. Automake
# exports AUTOM4TE to its Perl tools and must not receive argument fragments.
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
    marker=$("$candidate" --whp-autom4te-adapter-probe 2>/dev/null || true)
    [ "$marker" != whp-water-autom4te-adapter ] || exit 1

    banner=$("$candidate" --version 2>/dev/null) || exit 1
    case "$banner" in
        *'GNU Autoconf'*) ;;
        *) exit 1 ;;
    esac

    # Test the actual macro processor, not just an easy-to-fake banner.
    # --no-cache isolates this probe from an existing autom4te.cache.
    scratch=$(umask 077; mktemp -d "${TMPDIR:-/tmp}/water-autom4te.XXXXXXXX") || exit 1
    trap 'rm -rf -- "$scratch"' 0
    trap 'exit 1' 1 2 3 15
    printf 'AC_INIT([whp-water-autom4te],[1])\nAC_OUTPUT\n' > "$scratch/configure.ac"
    m4=$(/bin/sh "$root/scripts/whp-build/m4.sh" --print-path) || exit 1
    M4=$m4 "$candidate" --language=Autoconf --no-cache \
        --output="$scratch/configure" "$scratch/configure.ac" >/dev/null 2>&1 || exit 1
    [ -s "$scratch/configure" ] || exit 1
    grep -Fq 'whp-water-autom4te' "$scratch/configure" || exit 1
    printf '%s\n' "$candidate"
)

select_seed()
{
    # Invalid explicit choices must not silently fall back to some other seed.
    if [ -n "${WHP_AUTOMAKE_SEED_AUTOM4TE:-}" ]; then
        resolve_candidate "$WHP_AUTOMAKE_SEED_AUTOM4TE" || {
            error "WHP_AUTOMAKE_SEED_AUTOM4TE is not a functional GNU autom4te: $WHP_AUTOMAKE_SEED_AUTOM4TE"
            return 1
        }
        return 0
    fi
    if [ -n "${AUTOM4TE:-}" ]; then
        resolve_candidate "$AUTOM4TE" || {
            error "AUTOM4TE is not a functional GNU autom4te: $AUTOM4TE"
            return 1
        }
        return 0
    fi

    # The selected Autoconf may live outside PATH (e.g. a cached prefix).
    # Its matching autom4te is more reliable than an unrelated host version.
    seed_ac=${WHP_AUTOMAKE_SEED_AUTOCONF:-autoconf}
    case "$seed_ac" in
        *[[:space:]]*) ;;
        *)
            ac=$(command -v "$seed_ac" 2>/dev/null || true)
            if [ -n "$ac" ]; then
                case "$ac" in
                    */*)
                        sibling=${ac%/*}/autom4te
                        if resolved=$(resolve_candidate "$sibling"); then
                            printf '%s\n' "$resolved"
                            return 0
                        fi ;;
                esac
            fi ;;
    esac
    for name in autom4te /usr/bin/autom4te /opt/homebrew/bin/autom4te /usr/local/bin/autom4te; do
        if resolved=$(resolve_candidate "$name"); then
            printf '%s\n' "$resolved"
            return 0
        fi
    done
    error 'no working host autom4te found; install a matched Autoconf seed or set WHP_AUTOMAKE_SEED_AUTOM4TE'
    return 1
}

selected=$(select_seed) || exit 1
case "${1:-}" in
    --print-path|--print-seed) printf '%s\n' "$selected"; exit 0 ;;
    --check)
        printf 'WHP autom4te seed: %s\n' "$selected"
        exit 0 ;;
esac
exec "$selected" "$@"
