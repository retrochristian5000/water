#!/bin/sh
# Select a GNU M4 executable for Water host bootstrap tools.
# Invoke with --print-path to return a stable command path, or pass
# M4 arguments through to the selected executable.
set -eu

error()
{
    printf 'WHP M4: %s\n' "$*" >&2
    exit 1
}

# A configuration error in an explicit M4 must never silently fall back.
try_m4()
{
    candidate=$1
    resolved=$(command -v "$candidate" 2>/dev/null) || return 1
    [ -n "$resolved" ] && [ -f "$resolved" ] && [ -x "$resolved" ] || return 1
    case "$resolved" in
        /*) ;;
        */*)
            resolved=$(CDPATH= cd -P "$(dirname "$resolved")" && pwd)/$(basename "$resolved")
            ;;
        *) return 1 ;;
    esac
    # Prevent the obvious accidental recursive M4 override.
    [ "$resolved" != "$self" ] || return 1
    identity=$("$resolved" --gnu --version 2>/dev/null) || return 1
    case "$identity" in
        *'GNU M4'*) printf '%s\n' "$resolved"; return 0 ;;
        *) return 1 ;;
    esac
}

self=$(CDPATH= cd -P "$(dirname "$0")" && pwd)/$(basename "$0")
if [ -n "${M4:-}" ]; then
    selected=$(try_m4 "$M4") ||
        error "M4=$M4 is unavailable or not GNU M4 with --gnu support"
else
    selected=
    for name in gm4 m4; do
        selected=$(try_m4 "$name") && break
    done
    [ -n "$selected" ] ||
        error 'GNU M4 with --gnu support is required (install gm4 or set M4)'
fi

if [ "$#" -eq 1 ] && [ "$1" = --print-path ]; then
    printf '%s\n' "$selected"
    exit 0
fi
exec "$selected" "$@"
