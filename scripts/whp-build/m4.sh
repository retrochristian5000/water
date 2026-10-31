#!/bin/sh
# Select a GNU M4 executable for Water host bootstrap tools.
# --print-path returns the verified path; --check reports the selection.
# Otherwise forward the original arguments unchanged to GNU M4.
set -eu

error()
{
    printf 'WHP M4: %s\n' "$*" >&2
    exit 1
}

# Resolve symlinked executables (including symlinked parents). Use the
# physical path for both this adapter and its candidates to prevent loops.
physical_path()
(
    path=$1
    depth=0
    while :; do
        case "$path" in
            */*)
                path=$(CDPATH= cd -P -- "${path%/*}" && pwd -P)/${path##*/} || exit 1 ;;
            *) exit 1 ;;
        esac
        [ -L "$path" ] || break
        depth=$((depth + 1))
        [ "$depth" -le 16 ] || exit 1
        link=$(readlink "$path") || exit 1
        case "$link" in
            /*) path=$link ;;
            *) path=${path%/*}/$link ;;
        esac
    done
    [ -f "$path" ] && [ -x "$path" ] || exit 1
    printf '%s\n' "$path"
)

# A bad explicit selection must never silently fall back to PATH.
try_m4()
{
    candidate=$1
    resolved=$(command -v "$candidate" 2>/dev/null) || return 1
    [ -n "$resolved" ] || return 1
    resolved=$(physical_path "$resolved") || return 1
    [ "$resolved" != "$self" ] || return 1
    identity=$("$resolved" --gnu --version 2>/dev/null) || return 1
    case "$identity" in *'GNU M4'*) ;; *) return 1 ;; esac
    # Version-only shims are not functional M4. Verify real expansion,
    # bracket quoting and dnl as needed by the pinned Autotools forks.
    output=$(printf '%s\n' \
        'changequote([,])dnl' \
        'define([_WHP_M4_CHECK],[whp-water-m4-ok])dnl' \
        '_WHP_M4_CHECK' | "$resolved" --gnu 2>/dev/null) || return 1
    [ "$output" = whp-water-m4-ok ] || return 1
    printf '%s\n' "$resolved"
}

case "$0" in
    */*) entry=$0 ;;
    *) entry=$(command -v "$0" 2>/dev/null) ||
           error 'cannot resolve m4.sh adapter path' ;;
esac
self=$(physical_path "$entry") || error 'cannot resolve m4.sh adapter path'

if [ -n "${M4:-}" ]; then
    selected=$(try_m4 "$M4") ||
        error "M4=$M4 is not a working GNU M4 executable"
else
    selected=
    for name in gm4 m4; do
        selected=$(try_m4 "$name") && break
    done
    [ -n "$selected" ] ||
        error 'GNU M4 with working macro expansion is required (install gm4 or set M4)'
fi

if [ "$#" -eq 1 ]; then
    case "$1" in
        --print-path) printf '%s\n' "$selected"; exit 0 ;;
        --check) printf 'WHP GNU M4: %s\n' "$selected"; exit 0 ;;
    esac
fi
exec "$selected" "$@"
