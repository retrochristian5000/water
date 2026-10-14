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
            src_crt="$runtime_root/msvcrt-headers"
            src_headers="$runtime_root/$arch/include/c++/v1"
            src_archive="$runtime_root/$arch/provider/libwhp-libcxx.a"
            dst_headers="$destdir$includedir/wine/c++/$arch/v1"
            dst_crt="$destdir$includedir/wine/c++/$arch/crt"
            dst_lib="$destdir$libdir/wine/$arch-windows"

            [ -f "$src_headers/__config_site" ] ||
                { echo "error: LLVM libc++ headers are missing for $arch: $src_headers" >&2; exit 1; }
            [ -f "$src_archive" ] ||
                { echo "error: LLVM libc++ archive is missing for $arch: $src_archive" >&2; exit 1; }
            [ -f "$src_crt/corecrt.h" ] && [ -f "$src_crt/vcruntime_exception.h" ] ||
                { echo "error: filtered LLVM libc++ CRT staging is missing: $src_crt" >&2; exit 1; }
            # Modern consumers must never fall through to legacy __config,
            # <string>, <vector>, etc. Install a CRT-only tree per PE ABI.
            if [ -n "$(find "$src_crt" \( -type l -o \( -type f ! -name '*.h' ! -name '.whp-state' \) \) -print | sed -n '1p')" ]; then
                echo "error: filtered CRT staging has non-header/legacy C++ files: $src_crt" >&2
                exit 1
            fi

            mkdir -p "$dst_headers" "$dst_lib"
            rm -rf "$dst_crt"
            mkdir -p "$dst_crt"
            find "$src_crt" -type f -name '*.h' -print |
            while IFS= read -r file
            do
                rel=${file#"$src_crt"/}
                dir=${rel%/*}
                if [ "$dir" != "$rel" ]; then mkdir -p "$dst_crt/$dir"; fi
                cp -f "$file" "$dst_crt/$rel"
                chmod 644 "$dst_crt/$rel"
            done
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
