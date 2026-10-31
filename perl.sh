#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Perl interpreter bridge for Water's native Autotools bootstraps.
set -eu

case "${1:-}" in
    --whp-perl-adapter-probe)
        printf '%s\n' whp-water-perl-adapter
        exit 0 ;;
esac

error()
{
    printf 'WHP Perl: %s\n' "$*" >&2
}

resolve_perl()
(
    value=$1
    verbose=$2
    case "$value" in
        */*) candidate=$value ;;
        *[[:space:]]*)
            [ "$verbose" = 0 ] || error "Perl selection must be one executable, not a command: $value"
            exit 1 ;;
        *) candidate=$(command -v "$value" 2>/dev/null || true) ;;
    esac
    if [ -z "$candidate" ] || [ ! -x "$candidate" ]; then
        [ "$verbose" = 0 ] || error "Perl executable not found or not executable: $value"
        exit 1
    fi
    case "$candidate" in
        */*)
            directory=$(CDPATH= cd -P -- "${candidate%/*}" && pwd -P) || exit 1
            candidate=$directory/${candidate##*/} ;;
        *)
            [ "$verbose" = 0 ] || error "Perl selection is not a pathname: $candidate"
            exit 1 ;;
    esac
    marker=$("$candidate" --whp-perl-adapter-probe 2>/dev/null || true)
    if [ "$marker" = whp-water-perl-adapter ]; then
        [ "$verbose" = 0 ] || error "Perl selection recurses into perl.sh: $candidate"
        exit 1
    fi
    printf '%s\n' "$candidate"
)

probe_perl()
(
    candidate=$1
    verbose=$2
    # Exercise the interpreter, minimum version, and core modules used by
    # Automake/Autoconf. A --version banner alone is not an execution test.
    program='require 5.006; require File::Spec; require File::Path; require IO::File; require Cwd; require Fcntl; require Getopt::Long; print "whp-water-perl-ok\n";'
    if output=$("$candidate" -e "$program" 2>&1); then
        if [ "$output" = whp-water-perl-ok ]; then return 0; fi
        [ "$verbose" = 0 ] || error "Perl probe produced unexpected output: $candidate: $output"
    else
        status=$?
        if [ "$verbose" != 0 ]; then
            error "Perl 5.6+ or its required core modules failed (exit $status): $candidate"
            [ -z "$output" ] || printf '%s\n' "$output" >&2
        fi
    fi
    exit 1
)

try_perl()
{
    candidate=$(resolve_perl "$1" "$2") || return 1
    probe_perl "$candidate" "$2" || return 1
    printf '%s\n' "$candidate"
}

select_perl()
{
    if [ -n "${PERL:-}" ]; then
        try_perl "$PERL" 1 || { error 'explicit PERL override rejected'; return 1; }
        return
    fi
    for name in perl perl5; do
        if try_perl "$name" 0; then return 0; fi
    done
    error 'no working native Perl interpreter with required core modules was found'
    return 1
}

case "${1:-}" in
    --print-path|--print-perl) select_perl ;;
    --check)
        selected=$(select_perl) || exit 1
        printf 'WHP Perl interpreter: %s\n' "$selected" ;;
    *)
        selected=$(select_perl) || exit 1
        exec "$selected" "$@" ;;
esac
