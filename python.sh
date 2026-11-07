#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Native Python interpreter bridge for WATER bootstrap and host-side audits.
# Keep the generic name while checking the runtime actually needed by scripts.
set -eu

case "${1:-}" in
    --whp-python-adapter-probe)
        printf '%s\n' whp-water-python-adapter
        exit 0 ;;
esac

error()
{
    printf 'WHP Python: %s\n' "$*" >&2
}

resolve_python()
(
    requested=$1
    verbose=$2
    case "$requested" in
        */*) candidate=$requested ;;
        *[[:space:]]*)
            [ "$verbose" = 0 ] || error "Python selection must be one executable, not a command: $requested"
            exit 1 ;;
        *) candidate=$(command -v "$requested" 2>/dev/null || true) ;;
    esac
    if [ -z "$candidate" ] || [ ! -x "$candidate" ]; then
        [ "$verbose" = 0 ] || error "Python executable not found or not executable: $requested"
        exit 1
    fi
    case "$candidate" in
        /*) ;;
        */*)
            directory=$(CDPATH= cd -P -- "${candidate%/*}" && pwd -P) || exit 1
            candidate=$directory/${candidate##*/} ;;
        *)
            [ "$verbose" = 0 ] || error "Python selection is not a pathname: $candidate"
            exit 1 ;;
    esac
    marker=$("$candidate" --whp-python-adapter-probe 2>/dev/null || true)
    if [ "$marker" = whp-water-python-adapter ]; then
        [ "$verbose" = 0 ] || error "Python selection recurses into python.sh: $candidate"
        exit 1
    fi

    # Ninja's helper requires Python 3.7+ (future annotations and text=True).
    # Check the interpreter and its host-side standard library in one process.
    program='import sys, argparse, pathlib, subprocess, json, hashlib, shlex, shutil, tempfile; print("whp-water-python-ok" if sys.version_info >= (3, 7) else "whp-water-python-unsupported")'
    if output=$("$candidate" -c "$program" 2>&1); then
        if [ "$output" = whp-water-python-ok ]; then
            printf '%s\n' "$candidate"
            exit 0
        fi
    else
        status=$?
        [ "$verbose" = 0 ] || error "Python 3.7+ or required standard library failed (exit $status): $candidate"
        exit 1
    fi
    [ "$verbose" = 0 ] || error "Python 3.7+ probe returned unexpected output: $candidate: $output"
    exit 1
)

select_python()
{
    if [ -n "${WHP_PYTHON_SEED:-}" ]; then
        resolve_python "$WHP_PYTHON_SEED" 1 || {
            error 'explicit WHP_PYTHON_SEED override rejected'
            return 1
        }
        return 0
    fi
    if [ -n "${PYTHON:-}" ]; then
        resolve_python "$PYTHON" 1 || {
            error 'explicit PYTHON override rejected'
            return 1
        }
        return 0
    fi
    for name in python python3 python3.15 python3.14 python3.13 python3.12 python3.11 python3.10 python3.9 python3.8 python3.7; do
        if resolve_python "$name" 0; then return 0; fi
    done
    # A keg-only Homebrew Python may be installed but not on PATH.
    # Keep the explicit overrides strict, and probe only installed binaries.
    root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
    for item in python3:python python3.15:python@3.15 python3.14:python@3.14 python3.13:python@3.13 python3.12:python@3.12 python3.11:python@3.11 python3.10:python@3.10 python3.9:python@3.9 python3.8:python@3.8 python3.7:python@3.7; do
        name=${item%%:*}
        formula=${item#*:}
        brewed=$(/bin/sh "$root/scripts/whp-build/find-host-tool.sh" --brew-only "$name" "$formula" 2>/dev/null || true)
        [ -n "$brewed" ] || continue
        if resolve_python "$brewed" 0; then return 0; fi
    done
    error 'no working native Python 3.7+ interpreter with required standard library modules found'
    return 1
}

case "${1:-}" in
    --print-path|--print-python)
        select_python ;;
    --check)
        selected=$(select_python) || exit 1
        printf 'WHP Python interpreter: %s\n' "$selected" ;;
    *)
        selected=$(select_python) || exit 1
        exec "$selected" "$@" ;;
esac
