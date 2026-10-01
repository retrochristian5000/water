#!/bin/sh

set -eu

action=$1
provider=$2
runtime_root=$3
archs=$4
legacy_headers=$5
destdir=$6
includedir=$7
libdir=$8
legacy_source=$9

case "$includedir" in
    ''|/) echo "error: invalid include install directory: $includedir" >&2; exit 1 ;;
esac
case "$libdir" in
    ''|/) echo "error: invalid library install directory: $libdir" >&2; exit 1 ;;
esac

[ "$provider" = llvm ] || exit 0

remove_legacy_headers()
{
    [ "$legacy_headers" = 0 ] || return 0
    [ -d "$legacy_source" ] || return 0

    find "$legacy_source" -type f -print |
    while IFS= read -r file
    do
        rel=${file#"$legacy_source"/}
        base=${rel##*/}
        case "$rel:$base" in
            xlocinfo:xlocinfo) continue ;;
        esac
        case "$base" in
            *.*) continue ;;
        esac
        rm -f "$destdir$includedir/wine/msvcrt/$rel"
    done
}

case "$action" in
    install)
        remove_legacy_headers
        for arch in $archs
        do
            src_headers="$runtime_root/$arch/include/c++/v1"
            src_archive="$runtime_root/$arch/provider/libwhp-libcxx.a"
            dst_headers="$destdir$includedir/wine/c++/$arch/v1"
            dst_lib="$destdir$libdir/wine/$arch-windows"

            [ -f "$src_headers/__config_site" ] ||
                { echo "error: LLVM libc++ headers are missing for $arch: $src_headers" >&2; exit 1; }
            [ -f "$src_archive" ] ||
                { echo "error: LLVM libc++ archive is missing for $arch: $src_archive" >&2; exit 1; }

            mkdir -p "$dst_headers" "$dst_lib"
            find "$src_headers" -type f -print |
            while IFS= read -r file
            do
                rel=${file#"$src_headers"/}
                dir=${rel%/*}
                if [ "$dir" != "$rel" ]; then mkdir -p "$dst_headers/$dir"; fi
                cp -f "$file" "$dst_headers/$rel"
                chmod 644 "$dst_headers/$rel"
            done

            cp -f "$src_archive" "$dst_lib/libwhp-libcxx.a"
            chmod 644 "$dst_lib/libwhp-libcxx.a"
            rm -f "$dst_lib/libc++.a" "$dst_lib/libc++abi.a"
        done
        ;;
    uninstall)
        remove_legacy_headers
        for arch in $archs
        do
            rm -rf "$destdir$includedir/wine/c++/$arch"
            rm -f "$destdir$libdir/wine/$arch-windows/libwhp-libcxx.a"
        done
        ;;
    *)
        echo "error: unknown libc++ install action: $action" >&2
        exit 2
        ;;
esac
