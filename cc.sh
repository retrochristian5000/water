#!/bin/sh
# SPDX-License-Identifier: LGPL-2.1-or-later
# Select and verify a build-machine C compiler. Never select a Windows PE compiler.
set -eu

case "${1:-}" in
    --whp-cc-adapter-probe)
        printf '%s\n' whp-water-native-cc-adapter
        exit 0 ;;
    --cxx|--print-cxx|--check-cxx)
        echo 'error: cc.sh is a native C adapter, not a C++ compiler' >&2
        exit 2 ;;
esac

error()
{
    printf 'WHP C seed: %s\n' "$*" >&2
}

# Keep compiler discovery independent of PE flags and user header/linker paths.
probe_environment()
{
    unset CC CFLAGS CXXFLAGS CPPFLAGS LDFLAGS LIBS CROSSCFLAGS CROSSLDFLAGS
    unset CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH OBJC_INCLUDE_PATH
    unset COMPILER_PATH GCC_EXEC_PREFIX LIBRARY_PATH LD_LIBRARY_PATH LD_PRELOAD
    unset DYLD_LIBRARY_PATH DYLD_INSERT_LIBRARIES DYLD_FALLBACK_LIBRARY_PATH
}

apple_sdk()
(
    if [ -n "${DEVELOPER_DIR:-}" ]; then
        developer=$DEVELOPER_DIR
        case "$developer" in *.app) developer=$developer/Contents/Developer ;; esac
    else
        developer=$(xcode-select -p 2>/dev/null) || {
            error 'no active Xcode or Command Line Tools directory'; exit 1;
        }
    fi
    developer=$(CDPATH= cd -P -- "$developer" && pwd -P) || {
        error "developer directory not found: $developer"; exit 1;
    }
    command -v xcrun >/dev/null 2>&1 || { error 'xcrun not found'; exit 1; }
    if [ -n "${SDKROOT:-}" ] && [ -d "$SDKROOT" ]; then
        sdk=$SDKROOT
    else
        sdk=$(xcrun --sdk "${SDKROOT:-macosx}" --show-sdk-path 2>/dev/null) || {
            error 'cannot resolve the selected macOS SDK'; exit 1;
        }
    fi
    sdk=$(CDPATH= cd -P -- "$sdk" && pwd -P) || {
        error "macOS SDK not found: $sdk"; exit 1;
    }
    case "$sdk" in
        "$developer"/*/MacOSX*.sdk|"$developer"/SDKs/MacOSX*.sdk) ;;
        *) error "macOS SDK is not in selected developer tree: $sdk"; exit 1 ;;
    esac
    printf '%s\n' "$sdk"
)

compiler_path()
(
    value=$1
    verbose=$2
    case "$value" in
        */*) candidate=$value ;;
        *[[:space:]]*)
            [ "$verbose" = 0 ] || error "compiler command must be a single executable: $value"
            exit 1 ;;
        *) candidate=$(command -v "$value" 2>/dev/null || true) ;;
    esac
    if [ -z "$candidate" ] || [ ! -x "$candidate" ]; then
        [ "$verbose" = 0 ] || error "compiler is missing or not executable: $value"
        exit 1
    fi
    case "$candidate" in
        */*)
            directory=$(CDPATH= cd -P -- "${candidate%/*}" && pwd -P) || exit 1
            candidate=$directory/${candidate##*/} ;;
        *) [ "$verbose" = 0 ] || error "compiler is not a file path: $candidate"; exit 1 ;;
    esac
    # Reject recursion through aliases and other project's seed adapters.
    marker=$("$candidate" --whp-cc-adapter-probe 2>/dev/null || true)
    case "$marker" in
        whp-water-native-cc-adapter|whp-qemu-cc-seed-adapter|whp-qemu-cxx-seed-adapter)
            [ "$verbose" = 0 ] || error "compiler resolves to an adapter: $candidate"
            exit 1 ;;
    esac
    printf '%s\n' "$candidate"
)

native_probe()
(
    compiler=$1
    verbose=$2
    probe_environment
    if tmp=$(umask 077; mktemp -d "${TMPDIR:-/tmp}/water-cc.XXXXXXXX"); then :; else
        [ "$verbose" = 0 ] || error 'cannot create compiler probe directory'
        exit 1
    fi
    trap 'rm -rf -- "$tmp"' 0
    trap 'exit 1' 1 2 3 15
    printf '%s\n' '#include <stdlib.h>' '#include <unistd.h>' \
        'int main(void) { return access(".", F_OK) != 0; }' > "$tmp/probe.c"
    if [ "$(uname -s 2>/dev/null || true)" = Darwin ]; then
        sdk=$(apple_sdk) || exit 1
        set -- -isysroot "$sdk"
    else
        set --
    fi
    if "$compiler" "$@" "$tmp/probe.c" -o "$tmp/probe" > "$tmp/diagnostics" 2>&1; then :; else
        code=$?
        if [ "$verbose" != 0 ]; then
            error "native C compile/link failed (exit $code): $compiler"
            cat "$tmp/diagnostics" >&2
        fi
        exit 1
    fi
    if "$tmp/probe" > "$tmp/diagnostics" 2>&1; then :; else
        code=$?
        if [ "$verbose" != 0 ]; then
            error "native C executable failed (exit $code): $compiler"
            cat "$tmp/diagnostics" >&2
        fi
        exit 1
    fi
)

try_cc()
{
    requested=$1
    verbose=$2
    candidate=$(compiler_path "$requested" "$verbose") || return 1
    native_probe "$candidate" "$verbose" || return 1
    printf '%s\n' "$candidate"
}

select_cc()
{
    if [ -n "${WHP_CC_SEED:-}" ]; then
        try_cc "$WHP_CC_SEED" 1 || { error 'explicit WHP_CC_SEED rejected'; return 1; }
        return
    fi
    if [ -n "${CC_FOR_BUILD:-}" ]; then
        try_cc "$CC_FOR_BUILD" 1 || { error 'explicit CC_FOR_BUILD rejected'; return 1; }
        return
    fi
    if [ "$(uname -s 2>/dev/null || true)" = Darwin ]; then
        apple_sdk >/dev/null || return 1
        candidate=$(xcrun --sdk macosx --find clang 2>/dev/null) || {
            error 'could not find Apple Clang in selected developer tools'; return 1;
        }
        try_cc "$candidate" 1 || { error 'Apple Clang failed native preflight'; return 1; }
        return
    fi
    for candidate in clang cc gcc; do
        if try_cc "$candidate" 0; then return 0; fi
    done
    error 'no compiler could compile, link and run a native C executable'
    return 1
}

case "${1:-}" in
    --print-cc|--print-seed) select_cc; exit $? ;;
    --check)
        selected=$(select_cc) || exit 1
        printf 'WHP Water native C seed: %s\n' "$selected"
        exit 0 ;;
esac
selected=$(select_cc) || exit 1
if [ "$(uname -s 2>/dev/null || true)" = Darwin ]; then
    sdk=$(apple_sdk) || exit 1
    SDKROOT=$sdk
    export SDKROOT
    exec "$selected" -isysroot "$sdk" "$@"
fi
exec "$selected" "$@"
