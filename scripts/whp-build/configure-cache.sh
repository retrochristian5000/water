#!/bin/sh
# Reuse only a configure script whose inputs match an earlier, successfully
# generated pinned-Autoconf build. No Autoconf/M4/Automake is needed to check.
set -eu
[ "$#" -eq 3 ] || {
    echo 'usage: configure-cache.sh check|record SOURCE_DIR BUILD_DIR' >&2
    exit 2
}
mode=$1
root=$2
build=$3
manifest=$build/.whp-autoconf-inputs

signature()
{
    pin=$(git -C "$root" ls-tree HEAD -- toolchains/autoconf 2>/dev/null |
          awk '$2 == "commit" { print $3; exit }')
    [ -n "$pin" ] || return 1
    printf 'WHP_CONFIGURE_CACHE=1\nAUTOCONF_PIN=%s\n' "$pin"
    for input in configure.ac aclocal.m4 VERSION \
                 scripts/whp-build/ensure-autoconf.sh configure
    do
        [ -f "$root/$input" ] || return 1
        sum=$(cksum < "$root/$input") || return 1
        printf '%s %s\n' "$input" "$sum"
    done
}

case "$mode" in
    check)
        [ -f "$manifest" ] && [ -f "$build/.whp-autoconf-state" ] || exit 1
        current=$(signature) || exit 1
        [ "$(cat "$manifest")" = "$current" ]
        ;;
    record)
        current=$(signature) || {
            echo 'WHP configure cache: cannot record incomplete inputs' >&2
            exit 1
        }
        mkdir -p "$build"
        tmp=$manifest.tmp.$$
        trap 'rm -f "$tmp"' 0
        trap 'exit 1' 1 2 3 15
        (umask 077; printf '%s\n' "$current" > "$tmp")
        mv -f "$tmp" "$manifest"
        ;;
    *)
        echo 'usage: configure-cache.sh check|record SOURCE_DIR BUILD_DIR' >&2
        exit 2
        ;;
esac
