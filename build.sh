#!/bin/sh

set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${WHP_BUILD_DIR:-"$SOURCE_DIR/build"}
LLVM_SOURCE_DIR=${WHP_LLVM_SOURCE_DIR:-"$SOURCE_DIR/toolchains/llvm-project"}
WHP_SUBMODULES=${WHP_SUBMODULES:-1}
WHP_RECONFIGURE=${WHP_RECONFIGURE:-0}
AUTOCONF=${AUTOCONF:-autoconf}

die()
{
    printf 'error: %s\n' "$*" >&2
    exit 1
}

case "$WHP_SUBMODULES" in
    0|1) ;;
    *) die "WHP_SUBMODULES must be 0 or 1" ;;
esac

case "$WHP_RECONFIGURE" in
    0|1) ;;
    *) die "WHP_RECONFIGURE must be 0 or 1" ;;
esac

usage()
{
    cat <<EOF
Usage: ./build.sh [build|configure|reconfigure|clean|distclean|install|test|TARGET...]

Environment:
  WHP_BUILD_DIR         Out-of-tree build directory (default: ./build)
  WHP_BUILD_JOBS        Parallel build jobs (default: detected CPU count)
  WHP_LLVM_SOURCE_DIR   LLVM source tree (default: ./toolchains/llvm-project)
  WHP_LLVM_PREFIX       Built/installed LLVM prefix to prefer
  WHP_SUBMODULES        Initialize pinned submodules: 1 or 0 (default: 1)
  WHP_RECONFIGURE       Re-run configure before building: 1 or 0 (default: 0)
  AUTOCONF              Autoconf program used to generate ./configure

CC/CXX/AR/NM/RANLIB remain authoritative when explicitly set.
EOF
}

generate_configure()
{
    command -v "$AUTOCONF" >/dev/null 2>&1 ||
        die "Autoconf is required to generate ./configure (AUTOCONF=$AUTOCONF)"

    configure_tmp="$SOURCE_DIR/.configure.tmp.$$"
    rm -f "$configure_tmp"

    if ! (
        cd "$SOURCE_DIR"
        "$AUTOCONF" -o "$configure_tmp" configure.ac
    )
    then
        rm -f "$configure_tmp"
        die "failed to generate ./configure from configure.ac"
    fi

    chmod +x "$configure_tmp"

    if [ -f "$SOURCE_DIR/configure" ] &&
       cmp -s "$configure_tmp" "$SOURCE_DIR/configure"
    then
        rm -f "$configure_tmp"
        printf 'WHP configure script: up to date\n' >&2
    else
        mv -f "$configure_tmp" "$SOURCE_DIR/configure"
        printf 'WHP configure script: regenerated from configure.ac\n' >&2
    fi
}

init_submodules()
{
    if [ "$WHP_SUBMODULES" = 1 ] &&
       [ "$LLVM_SOURCE_DIR" = "$SOURCE_DIR/toolchains/llvm-project" ]; then
        command -v git >/dev/null 2>&1 ||
            die "git is required to initialize Water submodules"
        git -C "$SOURCE_DIR" submodule sync --recursive
        git -C "$SOURCE_DIR" submodule update --init --recursive
    fi

    [ -f "$LLVM_SOURCE_DIR/llvm/CMakeLists.txt" ] ||
        die "LLVM source tree is missing: $LLVM_SOURCE_DIR"
}

find_llvm_bin()
{
    if [ -n "${WHP_LLVM_PREFIX:-}" ]; then
        [ -x "$WHP_LLVM_PREFIX/bin/clang" ] ||
            die "WHP_LLVM_PREFIX does not contain bin/clang: $WHP_LLVM_PREFIX"
        printf '%s\n' "$WHP_LLVM_PREFIX/bin"
        return
    fi

    for dir in \
        "$LLVM_SOURCE_DIR/build/bin" \
        "$LLVM_SOURCE_DIR/build/Release/bin"; do
        if [ -x "$dir/clang" ]; then
            printf '%s\n' "$dir"
            return
        fi
    done

    clang_path=$(command -v clang 2>/dev/null || true)
    [ -n "$clang_path" ] || die "no usable clang was found"
    dirname -- "$clang_path"
}

setup_toolchain()
{
    LLVM_BIN=$(find_llvm_bin)

    if [ -z "${CC:-}" ]; then
        CC="$LLVM_BIN/clang"
    fi
    if [ -z "${CXX:-}" ]; then
        if [ -x "$LLVM_BIN/clang++" ]; then
            CXX="$LLVM_BIN/clang++"
        else
            CXX=$(command -v clang++ 2>/dev/null || true)
            [ -n "$CXX" ] || die "no usable clang++ was found"
        fi
    fi

    if [ -z "${AR:-}" ] && [ -x "$LLVM_BIN/llvm-ar" ]; then
        AR="$LLVM_BIN/llvm-ar"
    fi
    if [ -z "${NM:-}" ] && [ -x "$LLVM_BIN/llvm-nm" ]; then
        NM="$LLVM_BIN/llvm-nm"
    fi
    if [ -z "${RANLIB:-}" ] && [ -x "$LLVM_BIN/llvm-ranlib" ]; then
        RANLIB="$LLVM_BIN/llvm-ranlib"
    fi

    export CC CXX
    [ -z "${AR:-}" ] || export AR
    [ -z "${NM:-}" ] || export NM
    [ -z "${RANLIB:-}" ] || export RANLIB
    export LLVM_SOURCE_DIR
    WHP_LLVM_SOURCE_DIR=$LLVM_SOURCE_DIR
    export WHP_LLVM_SOURCE_DIR

    printf 'WHP LLVM source: %s\n' "$LLVM_SOURCE_DIR" >&2
    printf 'WHP C compiler: %s\n' "$CC" >&2
    printf 'WHP C++ compiler: %s\n' "$CXX" >&2
}

detect_jobs()
{
    if [ -n "${WHP_BUILD_JOBS:-}" ]; then
        jobs=$WHP_BUILD_JOBS
    else
        jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)
        case "$jobs" in
            ''|*[!0-9]*|0)
                jobs=$(sysctl -n hw.ncpu 2>/dev/null || true)
                ;;
        esac
        case "$jobs" in
            ''|*[!0-9]*|0) jobs=1 ;;
        esac
    fi

    case "$jobs" in
        ''|*[!0-9]*|0) die "WHP_BUILD_JOBS must be a positive integer" ;;
    esac
    printf '%s\n' "$jobs"
}

configure_build()
{
    mkdir -p "$BUILD_DIR"
    printf 'WHP configure: %s\n' "$BUILD_DIR" >&2
    (
        cd "$BUILD_DIR"
        "$SOURCE_DIR/configure" "$@"
    )
}

recheck_build()
{
    if [ -x "$BUILD_DIR/config.status" ]; then
        printf 'WHP configure: rechecking existing build options\n' >&2
        (
            cd "$BUILD_DIR"
            ./config.status --recheck
        )
    else
        configure_build
    fi
}

ensure_configured()
{
    if [ ! -f "$BUILD_DIR/Makefile" ] && [ ! -f "$BUILD_DIR/build.ninja" ]; then
        configure_build
    elif [ "$WHP_RECONFIGURE" = 1 ]; then
        recheck_build
    elif [ -f "$BUILD_DIR/config.status" ] &&
         [ "$SOURCE_DIR/configure" -nt "$BUILD_DIR/config.status" ]; then
        recheck_build
    fi
}

run_build()
{
    jobs=$(detect_jobs)

    if [ -f "$BUILD_DIR/build.ninja" ]; then
        ninja_cmd=${NINJA:-}
        if [ -z "$ninja_cmd" ]; then
            ninja_cmd=$(command -v ninja 2>/dev/null ||
                        command -v ninja-build 2>/dev/null || true)
        fi
        [ -n "$ninja_cmd" ] || die "build.ninja exists but Ninja was not found"
        "$ninja_cmd" -C "$BUILD_DIR" -j "$jobs" "$@"
        return
    fi

    make_cmd=${MAKE:-}
    if [ -z "$make_cmd" ]; then
        make_cmd=$(command -v gmake 2>/dev/null ||
                   command -v make 2>/dev/null || true)
    fi
    [ -n "$make_cmd" ] || die "make was not found"
    "$make_cmd" -C "$BUILD_DIR" -j"$jobs" "$@"
}

case "${1:-build}" in
    -h|--help|help)
        usage
        exit 0
        ;;
esac

generate_configure
init_submodules
setup_toolchain

case "${1:-build}" in
    configure)
        shift
        configure_build "$@"
        ;;
    reconfigure)
        shift
        if [ "$#" -gt 0 ]; then
            configure_build "$@"
        else
            recheck_build
        fi
        ;;
    build)
        if [ "$#" -gt 0 ]; then shift; fi
        ensure_configured
        run_build "$@"
        ;;
    *)
        ensure_configured
        run_build "$@"
        ;;
esac
