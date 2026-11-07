#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Resolve a host executable without modifying PATH or installing packages.
# On macOS, discover installed Homebrew formulae even if brew is not in PATH.
set -eu

brew_only=0
if [ "${1:-}" = --brew-only ]; then
    brew_only=1
    shift
fi
[ "$#" -eq 1 ] || [ "$#" -eq 2 ] || {
    printf 'usage: find-host-tool.sh [--brew-only] EXECUTABLE [FORMULA]\n' >&2
    exit 2
}
name=$1
formula=${2:-$name}
case "$name" in ''|*[!a-zA-Z0-9_.+-]*) exit 2 ;; esac
case "$formula" in ''|*[!a-zA-Z0-9_@.+-]*) exit 2 ;; esac

emit_executable()
(
    candidate=$1
    [ -f "$candidate" ] && [ -x "$candidate" ] || exit 1
    case "$candidate" in
        /*) ;;
        */*)
            directory=$(CDPATH= cd -P -- "${candidate%/*}" && pwd -P) || exit 1
            candidate=$directory/${candidate##*/} ;;
        *) exit 1 ;;
    esac
    printf '%s\n' "$candidate"
)

if [ "$brew_only" = 0 ]; then
    candidate=$(command -v "$name" 2>/dev/null || true)
    if [ -n "$candidate" ] && emit_executable "$candidate"; then
        exit 0
    fi
fi

# This fallback is macOS-only: never let a host package manager silently
# override ordinary Unix tools on other platforms.
[ "$(uname -s 2>/dev/null || true)" = Darwin ] || exit 1

# Homebrew's opt symlink is stable across upgrades and works without running
# brew, including when shellenv has not added its bin directory to PATH.
case "$(uname -m 2>/dev/null || true)" in
    arm64|aarch64) set -- /opt/homebrew /usr/local ;;
    *) set -- /usr/local /opt/homebrew ;;
esac
if [ -n "${HOMEBREW_PREFIX:-}" ]; then
    set -- "$HOMEBREW_PREFIX" "$@"
fi
for prefix do
    case "$prefix" in /*) ;; *) continue ;; esac
    for candidate in "$prefix/opt/$formula/bin/$name" "$prefix/bin/$name"; do
        if emit_executable "$candidate"; then exit 0; fi
    done
done

# The brew CLI is optional; consult it only for nonstandard prefixes. The
# --installed guard avoids treating a hypothetical formula prefix as a tool.
for brew in "$(command -v brew 2>/dev/null || true)" \
            /opt/homebrew/bin/brew /usr/local/bin/brew; do
    [ -f "$brew" ] && [ -x "$brew" ] || continue
    prefix=$("$brew" --prefix --installed "$formula" 2>/dev/null || true)
    case "$prefix" in /*) ;; *) continue ;; esac
    if emit_executable "$prefix/bin/$name"; then exit 0; fi
done
exit 1
