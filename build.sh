#!/bin/sh

set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${WHP_BUILD_DIR:-"$SOURCE_DIR/build"}
LLVM_SOURCE_DIR=${WHP_LLVM_SOURCE_DIR:-"$SOURCE_DIR/toolchains/llvm-project"}
LLVM_BOOTSTRAP_DIR=${WHP_LLVM_BUILD_DIR:-"$BUILD_DIR/llvm-bootstrap"}
LLVM_LINK_JOBS=${WHP_LLVM_LINK_JOBS:-2}
WHP_SUBMODULES=${WHP_SUBMODULES:-1}
WHP_RECONFIGURE=${WHP_RECONFIGURE:-0}
AUTOCONF=${AUTOCONF:-autoconf}
WHP_USER_CONFIG="$SOURCE_DIR/.whpconfig"
WHP_CONFIG_TOOL="$SOURCE_DIR/scripts/whp-config/config.py"
WHP_MENUCONFIG_TOOL="$SOURCE_DIR/scripts/whp-config/menuconfig.py"
WHP_MENUCONFIG_SHELL="$SOURCE_DIR/scripts/whp-config/menuconfig.sh"
WHP_MENU_SCHEMA="$SOURCE_DIR/scripts/whp-config/menu-options.def"
NINJA_BOOTSTRAP_TOOL="$SOURCE_DIR/scripts/ensure-ninja.py"
CONFIGURE_USER_ARGS_FILE="$BUILD_DIR/.whp-configure-args"
PROFILE_FILE="$BUILD_DIR/.whp-profile"
AUTOCONF_STATE_FILE="$BUILD_DIR/.whp-autoconf-state"
LLVM_BOOTSTRAP_CONFIG_FILE="$LLVM_BOOTSTRAP_DIR/.whp-config"
LLVM_BOOTSTRAP_STATE_FILE="$LLVM_BOOTSTRAP_DIR/.whp-state"
WHP_CONFIGURE_ARCHS=
WHP_CONFIGURE_ARCHS_SET=0

case "${1:-}" in
    configure|reconfigure)
        for whp_arg in "$@"; do
            case "$whp_arg" in
                --enable-archs=*)
                    WHP_CONFIGURE_ARCHS=${whp_arg#--enable-archs=}
                    WHP_CONFIGURE_ARCHS_SET=1
                    ;;
                --disable-archs)
                    WHP_CONFIGURE_ARCHS=none
                    WHP_CONFIGURE_ARCHS_SET=1
                    ;;
            esac
        done
        ;;
esac
unset whp_arg

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
Usage: ./build.sh [build|incremental|configure|reconfigure|menuconfig|clean|distclean|install|test|TARGET...]

Environment:
  WHP_BUILD_DIR         Out-of-tree build directory (default: ./build)
  WHP_BUILD_JOBS        Parallel build jobs (default: detected CPU count)
  WHP_LLVM_SOURCE_DIR   LLVM source tree (default: ./toolchains/llvm-project)
  WHP_LLVM_BUILD_DIR    Water LLVM bootstrap directory (default: ./build/llvm-bootstrap)
  WHP_LLVM_LINK_JOBS    Concurrent LLVM link jobs (default: 2)
  WHP_LLVM_PREFIX       Built/installed LLVM prefix to prefer
  NINJA_CMD              Explicit Ninja executable shared by LLVM and Water
  BOOTSTRAP_NINJA        Pinned WHP Ninja policy: auto, y, or n
  WHP_SUBMODULES        Initialize pinned submodules: 1 or 0 (default: 1)
  WHP_RECONFIGURE       Re-run configure before building: 1 or 0 (default: 0)
  AUTOCONF              Autoconf program used to generate ./configure

Run ./build.sh menuconfig to edit the persistent .whpconfig profile.
Explicit environment variables and explicit configure arguments override menu defaults.
CC/CXX/AR/NM/RANLIB remain authoritative when explicitly set.
EOF
}

find_python()
{
    for name in python3 python; do
        path=$(command -v "$name" 2>/dev/null || true)
        if [ -n "$path" ] &&
           "$path" -c 'import sys; raise SystemExit(0 if sys.version_info[0] >= 3 else 1)' >/dev/null 2>&1
        then
            printf '%s\n' "$path"
            return 0
        fi
    done
    return 1
}

run_menuconfig()
{
    shift
    python=$(find_python || true)
    if [ -n "$python" ]; then
        exec "$python" "$WHP_MENUCONFIG_TOOL" "$WHP_USER_CONFIG" "$@"
    fi
    exec /bin/sh "$WHP_MENUCONFIG_SHELL" "$WHP_USER_CONFIG" "$@"
}

load_whp_config()
{
    [ -f "$WHP_USER_CONFIG" ] || return 0

    python=$(find_python || true)
    if [ -n "$python" ]; then
        config_env=$("$python" "$WHP_CONFIG_TOOL" --shell "$WHP_USER_CONFIG") ||
            die "failed to read $WHP_USER_CONFIG"
        eval "$config_env"
        unset config_env
        return 0
    fi

    [ -f "$WHP_MENU_SCHEMA" ] || die "menu schema is missing: $WHP_MENU_SCHEMA"

    while IFS= read -r line || [ -n "$line" ]; do
        case "$line" in
            ''|'#'*) continue ;;
            WHP_CONFIG_VERSION=*) continue ;;
            *=*)
                key=${line%%=*}
                value=${line#*=}
                ;;
            *)
                die "invalid line in $WHP_USER_CONFIG: $line"
                ;;
        esac

        if ! awk -F '|' -v wanted="$key" '
            $0 !~ /^#/ && $1 == wanted { found = 1 }
            END { exit !found }
        ' "$WHP_MENU_SCHEMA"
        then
            printf 'warning: saved option %s is no longer recognized\n' "$key" >&2
            continue
        fi

        eval "already_set=\${$key+x}"
        if [ -z "$already_set" ]; then
            export "$key=$value"
        fi
    done < "$WHP_USER_CONFIG"
}

validate_profile()
{
    WATER_ARCHS_MODE=${WATER_ARCHS_MODE:-auto}
    WATER_LLVM_BOOTSTRAP=${WATER_LLVM_BOOTSTRAP:-auto}
    WATER_LLVM_BUILD_TYPE=${WATER_LLVM_BUILD_TYPE:-Release}
    WATER_LLVM_ASSERTIONS=${WATER_LLVM_ASSERTIONS:-n}
    WATER_LLVM_LEAN=${WATER_LLVM_LEAN:-y}
    WATER_LLVM_PCH=${WATER_LLVM_PCH:-n}
    WATER_COMPILER_CACHE=${WATER_COMPILER_CACHE:-auto}
    BOOTSTRAP_NINJA=${BOOTSTRAP_NINJA:-auto}

    case "$WATER_ARCHS_MODE" in
        auto|custom|none) ;;
        *) die "WATER_ARCHS_MODE must be auto, custom, or none" ;;
    esac
    case "$WATER_LLVM_BOOTSTRAP" in
        auto|y|n|0|1) ;;
        *) die "WATER_LLVM_BOOTSTRAP must be auto, y, or n" ;;
    esac
    case "$WATER_LLVM_BUILD_TYPE" in
        Release|RelWithDebInfo|Debug) ;;
        *) die "WATER_LLVM_BUILD_TYPE must be Release, RelWithDebInfo, or Debug" ;;
    esac
    case "$WATER_LLVM_ASSERTIONS" in
        y|n|0|1) ;;
        *) die "WATER_LLVM_ASSERTIONS must be y or n" ;;
    esac
    case "$WATER_LLVM_LEAN" in
        y|n|0|1) ;;
        *) die "WATER_LLVM_LEAN must be y or n" ;;
    esac
    case "$WATER_LLVM_PCH" in
        y|n|0|1) ;;
        *) die "WATER_LLVM_PCH must be y or n" ;;
    esac
    case "$LLVM_LINK_JOBS" in
        ''|*[!0-9]*|0) die "WHP_LLVM_LINK_JOBS must be a positive integer" ;;
    esac
    case "$WATER_COMPILER_CACHE" in
        auto|sccache|ccache|none) ;;
        *) die "WATER_COMPILER_CACHE must be auto, sccache, ccache, or none" ;;
    esac
    case "$BOOTSTRAP_NINJA" in
        auto|y|n|0|1) ;;
        *) die "BOOTSTRAP_NINJA must be auto, y, or n" ;;
    esac

    for var in \
        WATER_ARCH_I386 WATER_ARCH_X86_64 WATER_ARCH_ARM WATER_ARCH_AARCH64 \
        WATER_ARCH_ARM64EC WATER_ARCH_POWERPC
    do
        eval "value=\${$var:-y}"
        case "$value" in
            y|n|0|1) ;;
            *) die "$var must be y or n" ;;
        esac
    done

    for var in \
        WATER_WIN16 WATER_WIN64 WATER_TESTS WATER_BUILD_ID WATER_NINJA \
        WATER_MAINTAINER_MODE WATER_SAST WATER_SILENT_RULES WATER_WERROR \
        WATER_WITH_ALSA WATER_WITH_CAPI WATER_WITH_COREAUDIO WATER_WITH_CUPS \
        WATER_WITH_DBUS WATER_WITH_FFMPEG WATER_WITH_FONTCONFIG WATER_WITH_FREETYPE \
        WATER_WITH_GETTEXT WATER_WITH_GETTEXTPO WATER_WITH_GPHOTO WATER_WITH_GNUTLS \
        WATER_WITH_GSSAPI WATER_WITH_GSTREAMER WATER_WITH_HWLOC WATER_WITH_INOTIFY \
        WATER_WITH_KRB5 WATER_WITH_MINGW WATER_WITH_NETAPI WATER_WITH_OPENCL \
        WATER_WITH_OPENGL WATER_WITH_OSS WATER_WITH_PCAP WATER_WITH_PCSCLITE \
        WATER_WITH_PTHREAD WATER_WITH_PULSE WATER_WITH_SANE WATER_WITH_SDL \
        WATER_WITH_UDEV WATER_WITH_USB WATER_WITH_V4L2 WATER_WITH_VA \
        WATER_WITH_VULKAN WATER_WITH_WAYLAND WATER_WITH_XCOMPOSITE WATER_WITH_XCURSOR \
        WATER_WITH_XFIXES WATER_WITH_XINERAMA WATER_WITH_XINPUT WATER_WITH_XINPUT2 \
        WATER_WITH_XRANDR WATER_WITH_XRENDER WATER_WITH_XSHAPE WATER_WITH_XSHM \
        WATER_WITH_XXF86VM
    do
        eval "value=\${$var:-auto}"
        case "$value" in
            auto|y|n|0|1) ;;
            *) die "$var must be auto, y, or n" ;;
        esac
    done
}

autoconf_state_signature()
{
    autoconf_path=$(command -v "$AUTOCONF" 2>/dev/null || true)
    [ -n "$autoconf_path" ] ||
        die "Autoconf is required to generate ./configure (AUTOCONF=$AUTOCONF)"

    printf 'AUTOCONF=%s\n' "$autoconf_path"
    "$AUTOCONF" --version 2>/dev/null | sed -n '1p'
    cksum "$SOURCE_DIR/configure.ac"
    if [ -f "$SOURCE_DIR/aclocal.m4" ]; then
        cksum "$SOURCE_DIR/aclocal.m4"
    fi
    if [ -f "$SOURCE_DIR/configure" ]; then
        cksum "$SOURCE_DIR/configure"
    fi
}

record_autoconf_state()
{
    mkdir -p "$BUILD_DIR"
    tmp="$AUTOCONF_STATE_FILE.tmp.$$"
    autoconf_state_signature > "$tmp"
    mv -f "$tmp" "$AUTOCONF_STATE_FILE"
}

generate_configure()
{
    command -v "$AUTOCONF" >/dev/null 2>&1 ||
        die "Autoconf is required to generate ./configure (AUTOCONF=$AUTOCONF)"

    if [ -f "$SOURCE_DIR/configure" ] && [ -f "$AUTOCONF_STATE_FILE" ]; then
        current=$(autoconf_state_signature)
        previous=$(cat "$AUTOCONF_STATE_FILE")
        if [ "$current" = "$previous" ]; then
            printf 'WHP configure script: cached\n' >&2
            return 0
        fi
    fi

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

    if [ -f "$SOURCE_DIR/configure" ] && cmp -s "$configure_tmp" "$SOURCE_DIR/configure"; then
        rm -f "$configure_tmp"
        printf 'WHP configure script: up to date\n' >&2
    else
        mv -f "$configure_tmp" "$SOURCE_DIR/configure"
        printf 'WHP configure script: regenerated from configure.ac\n' >&2
    fi
    record_autoconf_state
}

init_submodules()
{
    if [ "$WHP_SUBMODULES" = 1 ] &&
       [ "$LLVM_SOURCE_DIR" = "$SOURCE_DIR/toolchains/llvm-project" ]; then
        command -v git >/dev/null 2>&1 || die "git is required to initialize Water submodules"
        git -C "$SOURCE_DIR" submodule sync --recursive
        git -C "$SOURCE_DIR" submodule update --init --recursive
    fi
}

detect_jobs()
{
    if [ -n "${WHP_BUILD_JOBS:-}" ]; then
        jobs=$WHP_BUILD_JOBS
    else
        jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)
        case "$jobs" in
            ''|*[!0-9]*|0) jobs=$(sysctl -n hw.ncpu 2>/dev/null || true) ;;
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

darwin_sdkroot()
{
    case "$(uname -s 2>/dev/null || true)" in
        Darwin) ;;
        *) return 0 ;;
    esac

    xcrun_cmd=$(command -v xcrun 2>/dev/null || true)
    [ -n "$xcrun_cmd" ] ||
        die "xcrun is required to locate the macOS SDK"

    if [ -n "${SDKROOT:-}" ]; then
        if [ -d "$SDKROOT" ]; then
            printf '%s\n' "$SDKROOT"
            return 0
        fi
        sdkroot=$("$xcrun_cmd" --sdk "$SDKROOT" --show-sdk-path 2>/dev/null || true)
    else
        sdkroot=$("$xcrun_cmd" --sdk macosx --show-sdk-path 2>/dev/null || true)
    fi

    [ -n "$sdkroot" ] && [ -d "$sdkroot" ] ||
        die "could not resolve a usable macOS SDK (SDKROOT=${SDKROOT:-auto})"
    printf '%s\n' "$sdkroot"
}

compiler_has_assert_h()
{
    compiler=$1
    language=$2
    printf '#include <assert.h>\nint main(void) { return 0; }\n' |
        "$compiler" -x "$language" -fsyntax-only - >/dev/null 2>&1
}

shell_quote()
{
    printf "'"
    printf '%s' "$1" | sed "s/'/'\\\\''/g"
    printf "'"
}

write_darwin_compiler_wrapper()
{
    name=$1
    compiler=$2
    sdkroot=$3
    wrapper_dir="$BUILD_DIR/.whp-host-toolchain"
    wrapper="$wrapper_dir/$name"
    tmp="$wrapper.tmp.$$"

    mkdir -p "$wrapper_dir"
    compiler_q=$(shell_quote "$compiler")
    sdkroot_q=$(shell_quote "$sdkroot")
    {
        printf '%s\n' '#!/bin/sh'
        printf 'exec %s -isysroot %s "$@"\n' "$compiler_q" "$sdkroot_q"
    } > "$tmp"
    chmod +x "$tmp"
    mv -f "$tmp" "$wrapper"
    printf '%s\n' "$wrapper"
}

prepare_ninja()
{
    ninja_cmd=
    ninja_required=0
    case "${WATER_NINJA:-auto}" in
        y|1) ninja_required=1 ;;
    esac

    if [ -n "${NINJA_CMD:-}" ]; then
        ninja_cmd=$NINJA_CMD
    elif [ -n "${NINJA:-}" ]; then
        ninja_cmd=$NINJA
    else
        case "$BOOTSTRAP_NINJA" in
            y|1)
                python=$(find_python || true)
                [ -n "$python" ] ||
                    die "BOOTSTRAP_NINJA=y requires Python 3 to bootstrap the pinned Ninja fork"
                ninja_cmd=$(WATER_COMPILER_CACHE="$WATER_COMPILER_CACHE" WHP_SUBMODULES="$WHP_SUBMODULES" \
                    "$python" "$NINJA_BOOTSTRAP_TOOL" --build-dir "$BUILD_DIR") ||
                    die "pinned WHP Ninja bootstrap failed"
                ;;
            n|0)
                ninja_cmd=$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)
                ;;
            auto)
                python=$(find_python || true)
                case "$(uname -s 2>/dev/null || true)" in
                    Darwin)
                        if [ -n "$python" ]; then
                            ninja_cmd=$(WATER_COMPILER_CACHE="$WATER_COMPILER_CACHE" WHP_SUBMODULES="$WHP_SUBMODULES" \
                                "$python" "$NINJA_BOOTSTRAP_TOOL" --build-dir "$BUILD_DIR" || true)
                        fi
                        [ -n "$ninja_cmd" ] ||
                            ninja_cmd=$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)
                        ;;
                    *)
                        ninja_cmd=$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)
                        if [ -z "$ninja_cmd" ] && [ -n "$python" ]; then
                            ninja_cmd=$(WATER_COMPILER_CACHE="$WATER_COMPILER_CACHE" WHP_SUBMODULES="$WHP_SUBMODULES" \
                                "$python" "$NINJA_BOOTSTRAP_TOOL" --build-dir "$BUILD_DIR" || true)
                        fi
                        ;;
                esac
                ;;
        esac
    fi

    if [ -z "$ninja_cmd" ]; then
        [ "$ninja_required" = 0 ] ||
            die "Water Ninja output was requested but no usable Ninja executable is available"
        printf 'WHP Ninja: unavailable; Make remains available\n' >&2
        return 0
    fi

    "$ninja_cmd" --version >/dev/null 2>&1 ||
        die "selected Ninja executable is not usable: $ninja_cmd"

    NINJA_CMD=$ninja_cmd
    NINJA=$ninja_cmd
    case "$NINJA_CMD" in
        */*)
            ninja_dir=$(dirname -- "$NINJA_CMD")
            PATH="$ninja_dir:$PATH"
            unset ninja_dir
            ;;
    esac
    export NINJA_CMD NINJA PATH
    printf 'WHP Ninja: %s\n' "$NINJA_CMD" >&2
}

llvm_add_target()
{
    target=$1
    case ";$llvm_targets;" in
        *";$target;"*) ;;
        *)
            if [ -n "$llvm_targets" ]; then
                llvm_targets="$llvm_targets;$target"
            else
                llvm_targets=$target
            fi
            ;;
    esac
}

llvm_add_arch_list()
{
    arch_list=$1
    old_ifs=$IFS
    IFS=" ,"
    set -- $arch_list
    IFS=$old_ifs

    for arch
    do
        case "$arch" in
            ""|no|none) ;;
            i386|x86_64) llvm_add_target X86 ;;
            arm) llvm_add_target ARM ;;
            aarch64) llvm_add_target AArch64 ;;
            arm64ec)
                llvm_add_target AArch64
                # Water configure adds x86_64 as the ARM64EC companion PE arch.
                llvm_add_target X86
                ;;
            powerpc) llvm_add_target PowerPC ;;
            *) die "unknown architecture in --enable-archs: $arch" ;;
        esac
    done
}

select_saved_configure_archs()
{
    saved_archs=
    saved_archs_set=0
    [ -f "$CONFIGURE_USER_ARGS_FILE" ] || return 0

    while IFS= read -r arg || [ -n "$arg" ]; do
        case "$arg" in
            --enable-archs=*)
                saved_archs=${arg#--enable-archs=}
                saved_archs_set=1
                ;;
            --disable-archs)
                saved_archs=none
                saved_archs_set=1
                ;;
        esac
    done < "$CONFIGURE_USER_ARGS_FILE"

    [ "$saved_archs_set" = 1 ] && printf '%s\n' "$saved_archs"
}

select_llvm_targets()
{
    llvm_targets=
    host_arch=$(uname -m 2>/dev/null || true)
    case "$host_arch" in
        i386|i486|i586|i686|x86_64|amd64) llvm_add_target X86 ;;
        arm|armv6*|armv7*)                llvm_add_target ARM ;;
        arm64|aarch64)                    llvm_add_target AArch64 ;;
        ppc|ppc64|ppc64le|powerpc*)       llvm_add_target PowerPC ;;
        *)
            llvm_add_target X86
            llvm_add_target ARM
            llvm_add_target AArch64
            llvm_add_target PowerPC
            ;;
    esac

    if [ "$WHP_CONFIGURE_ARCHS_SET" = 1 ]; then
        llvm_add_arch_list "$WHP_CONFIGURE_ARCHS"
    else
        saved_archs=$(select_saved_configure_archs || true)
        if [ -n "$saved_archs" ]; then
            llvm_add_arch_list "$saved_archs"
        elif [ "$WATER_ARCHS_MODE" = custom ]; then
            for item in \
                WATER_ARCH_I386:X86 WATER_ARCH_X86_64:X86 WATER_ARCH_ARM:ARM \
                WATER_ARCH_AARCH64:AArch64 WATER_ARCH_POWERPC:PowerPC
            do
                var=${item%%:*}
                backend=${item#*:}
                eval "enabled=\${$var:-y}"
                case "$enabled" in y|1) llvm_add_target "$backend" ;; esac
            done

            eval "arm64ec_enabled=\${WATER_ARCH_ARM64EC:-y}"
            case "$arm64ec_enabled" in
                y|1)
                    llvm_add_target AArch64
                    llvm_add_target X86
                    ;;
            esac
        fi
    fi

    printf '%s\n' "$llvm_targets"
}

select_llvm_cache()
{
    llvm_cache=
    case "$WATER_COMPILER_CACHE" in
        auto)
            llvm_cache=$(command -v sccache 2>/dev/null || true)
            [ -n "$llvm_cache" ] || llvm_cache=$(command -v ccache 2>/dev/null || true)
            ;;
        sccache|ccache)
            llvm_cache=$(command -v "$WATER_COMPILER_CACHE" 2>/dev/null || true)
            [ -n "$llvm_cache" ] ||
                die "requested compiler cache is not installed: $WATER_COMPILER_CACHE"
            ;;
        none) ;;
    esac
    printf '%s\n' "$llvm_cache"
}

llvm_source_state_signature()
{
    git_cmd=$(command -v git 2>/dev/null || true)
    [ -n "$git_cmd" ] || return 1
    llvm_revision=$("$git_cmd" -C "$LLVM_SOURCE_DIR" rev-parse HEAD 2>/dev/null) ||
        return 1

    printf 'REV=%s\n' "$llvm_revision"
    if "$git_cmd" -C "$LLVM_SOURCE_DIR" diff --quiet --ignore-submodules=dirty HEAD -- 2>/dev/null; then
        printf 'DIFF=clean\n'
    else
        printf 'DIFF='
        "$git_cmd" -C "$LLVM_SOURCE_DIR" diff --binary --no-ext-diff \
            --ignore-submodules=dirty HEAD -- 2>/dev/null |
            cksum | awk '{ printf "%s:%s\n", $1, $2 }'
    fi
}

llvm_bootstrap_config_signature()
{
    llvm_targets_sig=$(select_llvm_targets)
    llvm_cache_sig=$(select_llvm_cache)
    llvm_sdkroot_sig=$(darwin_sdkroot)

    printf '%s\n' \
        "WATER_LLVM_BUILD_TYPE=$WATER_LLVM_BUILD_TYPE" \
        "WATER_LLVM_ASSERTIONS=$WATER_LLVM_ASSERTIONS" \
        "WATER_LLVM_LEAN=$WATER_LLVM_LEAN" \
        "WATER_LLVM_PCH=$WATER_LLVM_PCH" \
        "WHP_LLVM_LINK_JOBS=$LLVM_LINK_JOBS" \
        "WATER_COMPILER_CACHE=$WATER_COMPILER_CACHE" \
        "LLVM_TARGETS=$llvm_targets_sig" \
        "LLVM_CACHE=$llvm_cache_sig" \
        "LLVM_SDKROOT=$llvm_sdkroot_sig" \
        "NINJA_CMD=${NINJA_CMD:-${NINJA:-}}"
}

llvm_bootstrap_state_signature()
{
    if llvm_source_state=$(llvm_source_state_signature 2>/dev/null); then
        printf '%s\n' "$llvm_source_state"
    else
        printf 'REV=unknown\nDIFF=unknown\n'
    fi
    llvm_bootstrap_config_signature
}

record_llvm_bootstrap_state()
{
    tmp="$LLVM_BOOTSTRAP_STATE_FILE.tmp.$$"
    llvm_bootstrap_state_signature > "$tmp"
    mv -f "$tmp" "$LLVM_BOOTSTRAP_STATE_FILE"
}

record_llvm_bootstrap_config()
{
    tmp="$LLVM_BOOTSTRAP_CONFIG_FILE.tmp.$$"
    llvm_bootstrap_config_signature > "$tmp"
    mv -f "$tmp" "$LLVM_BOOTSTRAP_CONFIG_FILE"
}

llvm_bootstrap_needs_update()
{
    [ -x "$LLVM_BOOTSTRAP_DIR/bin/clang" ] || return 0
    [ -f "$LLVM_BOOTSTRAP_STATE_FILE" ] || return 0

    llvm_source_state_signature >/dev/null 2>&1 || return 0

    current=$(llvm_bootstrap_state_signature)
    previous=$(cat "$LLVM_BOOTSTRAP_STATE_FILE")
    [ "$current" != "$previous" ]
}

bootstrap_llvm()
{
    [ -f "$LLVM_SOURCE_DIR/llvm/CMakeLists.txt" ] ||
        die "LLVM source tree is missing: $LLVM_SOURCE_DIR"

    cmake_cmd=$(command -v cmake 2>/dev/null || true)
    [ -n "$cmake_cmd" ] || die "CMake is required to bootstrap LLVM"

    case "$WATER_LLVM_ASSERTIONS" in
        y|1) llvm_assertions=ON ;;
        *) llvm_assertions=OFF ;;
    esac
    case "$WATER_LLVM_PCH" in
        y|1) llvm_disable_pch=OFF ;;
        *) llvm_disable_pch=ON ;;
    esac

    llvm_targets=$(select_llvm_targets)
    llvm_cache=$(select_llvm_cache)
    llvm_lld_backends="COFF;MinGW"

    mkdir -p "$LLVM_BOOTSTRAP_DIR"

    set -- \
        -S "$LLVM_SOURCE_DIR/llvm" \
        -B "$LLVM_BOOTSTRAP_DIR" \
        "-DCMAKE_BUILD_TYPE=$WATER_LLVM_BUILD_TYPE" \
        "-DCMAKE_DISABLE_PRECOMPILE_HEADERS=$llvm_disable_pch" \
        "-DCMAKE_C_COMPILER_LAUNCHER=$llvm_cache" \
        "-DCMAKE_CXX_COMPILER_LAUNCHER=$llvm_cache" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
        -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
        "-DLLVM_ENABLE_ASSERTIONS=$llvm_assertions" \
        "-DLLVM_ENABLE_PROJECTS=clang;lld" \
        "-DLLVM_TARGETS_TO_BUILD=$llvm_targets" \
        "-DLLD_ENABLE_BACKENDS=$llvm_lld_backends" \
        -DLLVM_APPEND_VC_REV=OFF \
        -DLLVM_ENABLE_LTO=OFF \
        -DLLVM_ENABLE_FATLTO=OFF \
        -DLLVM_BUILD_INSTRUMENTED=OFF \
        -DLLVM_ENABLE_MODULES=OFF \
        -DLLVM_ENABLE_PLUGINS=OFF

    llvm_sdkroot=$(darwin_sdkroot)
    if [ -n "$llvm_sdkroot" ]; then
        set -- "$@" "-DCMAKE_OSX_SYSROOT=$llvm_sdkroot"
        printf 'WHP LLVM macOS SDK: %s\n' "$llvm_sdkroot" >&2
    fi

    case "$WATER_LLVM_LEAN" in
        y|1)
            set -- "$@" \
                -DLLVM_BUILD_TOOLS=OFF \
                -DLLVM_BUILD_UTILS=OFF \
                -DLLVM_BUILD_RUNTIMES=OFF \
                -DLLVM_INCLUDE_TESTS=OFF \
                -DLLVM_INCLUDE_EXAMPLES=OFF \
                -DLLVM_INCLUDE_BENCHMARKS=OFF \
                -DLLVM_INCLUDE_DOCS=OFF \
                -DLLVM_INCLUDE_UTILS=OFF \
                -DLLVM_INCLUDE_RUNTIMES=OFF \
                -DLLVM_ENABLE_BINDINGS=OFF \
                -DLLVM_ENABLE_TELEMETRY=OFF \
                -DCLANG_BUILD_TOOLS=OFF \
                -DCLANG_INCLUDE_TESTS=OFF \
                -DCLANG_ENABLE_STATIC_ANALYZER=ON
            ;;
    esac

    llvm_ninja_generator=0
    if [ -f "$LLVM_BOOTSTRAP_DIR/CMakeCache.txt" ]; then
        if grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja' "$LLVM_BOOTSTRAP_DIR/CMakeCache.txt"; then
            llvm_ninja_generator=1
        fi
    else
        ninja_cmd=${NINJA_CMD:-${NINJA:-}}
        if [ -z "$ninja_cmd" ]; then
            ninja_cmd=$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)
        fi
        if [ -n "$ninja_cmd" ]; then
            set -- "$@" -G Ninja "-DCMAKE_MAKE_PROGRAM=$ninja_cmd"
            llvm_ninja_generator=1
        fi
    fi

    if [ "$llvm_ninja_generator" = 1 ]; then
        set -- "$@" "-DLLVM_PARALLEL_LINK_JOBS=$LLVM_LINK_JOBS"
    fi

    llvm_configure=1
    if [ -f "$LLVM_BOOTSTRAP_DIR/CMakeCache.txt" ] &&
       [ -f "$LLVM_BOOTSTRAP_CONFIG_FILE" ]; then
        current=$(llvm_bootstrap_config_signature)
        previous=$(cat "$LLVM_BOOTSTRAP_CONFIG_FILE")
        if [ "$current" = "$previous" ]; then
            llvm_configure=0
        fi
    fi

    printf 'WHP LLVM targets: %s\n' "$llvm_targets" >&2
    printf 'WHP LLVM LLD backends: %s\n' "$llvm_lld_backends" >&2
    if [ -n "$llvm_cache" ]; then
        printf 'WHP LLVM compiler cache: %s\n' "$llvm_cache" >&2
    else
        printf 'WHP LLVM compiler cache: disabled\n' >&2
    fi
    printf 'WHP LLVM lean profile: %s\n' "$WATER_LLVM_LEAN" >&2

    if [ "$llvm_configure" = 1 ]; then
        "$cmake_cmd" "$@"
        record_llvm_bootstrap_config
    else
        printf 'WHP LLVM CMake: cached\n' >&2
    fi

    jobs=$(detect_jobs)
    printf 'WHP LLVM bootstrap: incremental %s\n' "$LLVM_BOOTSTRAP_DIR" >&2
    "$cmake_cmd" --build "$LLVM_BOOTSTRAP_DIR" --parallel "$jobs" \
        --target clang lld llvm-ar llvm-nm llvm-ranlib llvm-strip
    record_llvm_bootstrap_state
}
prepare_llvm_toolchain()
{
    if [ -n "${WHP_LLVM_PREFIX:-}" ]; then
        return
    fi

    case "$WATER_LLVM_BOOTSTRAP" in
        y|1)
            bootstrap_llvm
            ;;
        n|0)
            return
            ;;
        auto)
            if [ -n "${CC:-}" ]; then
                return
            fi
            if [ -x "$LLVM_BOOTSTRAP_DIR/bin/clang" ]; then
                if llvm_bootstrap_needs_update; then
                    printf 'WHP LLVM bootstrap: source/config changed; updating incrementally\n' >&2
                    bootstrap_llvm
                else
                    printf 'WHP LLVM bootstrap: cached\n' >&2
                fi
                return
            fi
            if [ -x "$LLVM_SOURCE_DIR/build/bin/clang" ] ||
               [ -x "$LLVM_SOURCE_DIR/build/Release/bin/clang" ] ||
               command -v clang >/dev/null 2>&1
            then
                return
            fi
            bootstrap_llvm
            ;;
    esac
}

find_llvm_bin()
{
    if [ -n "${WHP_LLVM_PREFIX:-}" ]; then
        [ -x "$WHP_LLVM_PREFIX/bin/clang" ] ||
            die "WHP_LLVM_PREFIX does not contain bin/clang: $WHP_LLVM_PREFIX"
        printf '%s\n' "$WHP_LLVM_PREFIX/bin"
        return 0
    fi

    for dir in \
        "$LLVM_BOOTSTRAP_DIR/bin" \
        "$LLVM_SOURCE_DIR/build/bin" \
        "$LLVM_SOURCE_DIR/build/Release/bin"
    do
        if [ -x "$dir/clang" ]; then
            printf '%s\n' "$dir"
            return 0
        fi
    done

    clang_path=$(command -v clang 2>/dev/null || true)
    [ -n "$clang_path" ] || return 1
    dirname -- "$clang_path"
}

setup_toolchain()
{
    LLVM_BIN=$(find_llvm_bin || true)
    WHP_LLVM_TOOLCHAIN_STATE=
    if [ "$LLVM_BIN" = "$LLVM_BOOTSTRAP_DIR/bin" ] &&
       [ -f "$LLVM_BOOTSTRAP_STATE_FILE" ]; then
        WHP_LLVM_TOOLCHAIN_STATE=$(cksum "$LLVM_BOOTSTRAP_STATE_FILE" |
            awk '{ printf "%s:%s", $1, $2 }')
    fi
    export WHP_LLVM_TOOLCHAIN_STATE

    whp_auto_cc=0
    whp_auto_cxx=0

    if [ -z "${CC:-}" ]; then
        [ -n "$LLVM_BIN" ] || die "no usable clang was found"
        CC="$LLVM_BIN/clang"
        whp_auto_cc=1
    fi
    if [ -z "${CXX:-}" ]; then
        if [ -n "$LLVM_BIN" ] && [ -x "$LLVM_BIN/clang++" ]; then
            CXX="$LLVM_BIN/clang++"
        else
            CXX=$(command -v clang++ 2>/dev/null || true)
            [ -n "$CXX" ] || die "no usable clang++ was found"
        fi
        whp_auto_cxx=1
    fi

    WHP_DARWIN_SDKROOT=
    WHP_HOST_CC_REAL=
    WHP_HOST_CXX_REAL=
    case "$(uname -s 2>/dev/null || true)" in
        Darwin)
            cc_has_assert=1
            cxx_has_assert=1
            if [ "$whp_auto_cc" = 1 ]; then
                compiler_has_assert_h "$CC" c || cc_has_assert=0
            fi
            if [ "$whp_auto_cxx" = 1 ]; then
                compiler_has_assert_h "$CXX" c++ || cxx_has_assert=0
            fi

            if [ "$cc_has_assert" = 0 ] || [ "$cxx_has_assert" = 0 ]; then
                WHP_DARWIN_SDKROOT=$(darwin_sdkroot)

                if [ "$cc_has_assert" = 0 ]; then
                    WHP_HOST_CC_REAL=$CC
                    CC=$(write_darwin_compiler_wrapper clang "$WHP_HOST_CC_REAL" "$WHP_DARWIN_SDKROOT")
                    compiler_has_assert_h "$CC" c ||
                        die "LLVM C compiler still cannot find assert.h with macOS SDK $WHP_DARWIN_SDKROOT"
                fi
                if [ "$cxx_has_assert" = 0 ]; then
                    WHP_HOST_CXX_REAL=$CXX
                    CXX=$(write_darwin_compiler_wrapper clang++ "$WHP_HOST_CXX_REAL" "$WHP_DARWIN_SDKROOT")
                    compiler_has_assert_h "$CXX" c++ ||
                        die "LLVM C++ compiler still cannot find assert.h with macOS SDK $WHP_DARWIN_SDKROOT"
                fi

                printf 'WHP host compiler SDK wrapper: %s\n' "$WHP_DARWIN_SDKROOT" >&2
            fi
            ;;
    esac

    export WHP_DARWIN_SDKROOT WHP_HOST_CC_REAL WHP_HOST_CXX_REAL

    if [ -n "$LLVM_BIN" ]; then
        if [ -z "${AR:-}" ] && [ -x "$LLVM_BIN/llvm-ar" ]; then AR="$LLVM_BIN/llvm-ar"; fi
        if [ -z "${NM:-}" ] && [ -x "$LLVM_BIN/llvm-nm" ]; then NM="$LLVM_BIN/llvm-nm"; fi
        if [ -z "${RANLIB:-}" ] && [ -x "$LLVM_BIN/llvm-ranlib" ]; then RANLIB="$LLVM_BIN/llvm-ranlib"; fi
    fi

    export CC CXX
    [ -z "${AR:-}" ] || export AR
    [ -z "${NM:-}" ] || export NM
    [ -z "${RANLIB:-}" ] || export RANLIB
    export LLVM_SOURCE_DIR
    WHP_LLVM_SOURCE_DIR=$LLVM_SOURCE_DIR
    export WHP_LLVM_SOURCE_DIR

    printf 'WHP LLVM source: %s\n' "$LLVM_SOURCE_DIR" >&2
    [ -z "$LLVM_BIN" ] || printf 'WHP LLVM bin: %s\n' "$LLVM_BIN" >&2
    printf 'WHP C compiler: %s\n' "$CC" >&2
    printf 'WHP C++ compiler: %s\n' "$CXX" >&2
}

profile_signature()
{
    printf '%s\n' \
        "WATER_ARCHS_MODE=${WATER_ARCHS_MODE:-auto}" \
        "WATER_LLVM_BOOTSTRAP=${WATER_LLVM_BOOTSTRAP:-auto}" \
        "WATER_LLVM_BUILD_TYPE=${WATER_LLVM_BUILD_TYPE:-Release}" \
        "WATER_LLVM_ASSERTIONS=${WATER_LLVM_ASSERTIONS:-n}" \
        "WATER_LLVM_LEAN=${WATER_LLVM_LEAN:-y}" \
        "WATER_LLVM_PCH=${WATER_LLVM_PCH:-n}" \
        "WHP_LLVM_LINK_JOBS=$LLVM_LINK_JOBS" \
        "WATER_COMPILER_CACHE=${WATER_COMPILER_CACHE:-auto}" \
        "BOOTSTRAP_NINJA=${BOOTSTRAP_NINJA:-auto}" \
        "NINJA_CMD=${NINJA_CMD:-}" \
        "WHP_DARWIN_SDKROOT=${WHP_DARWIN_SDKROOT:-}" \
        "WHP_HOST_CC_REAL=${WHP_HOST_CC_REAL:-}" \
        "WHP_HOST_CXX_REAL=${WHP_HOST_CXX_REAL:-}" \
        "WHP_LLVM_TOOLCHAIN_STATE=${WHP_LLVM_TOOLCHAIN_STATE:-}" \
        "WATER_SYSTEM_DLLPATH=${WATER_SYSTEM_DLLPATH:-auto}" \
        "WATER_WINE_TOOLS=${WATER_WINE_TOOLS:-auto}" \
        "WATER_WINE64=${WATER_WINE64:-auto}" \
        "CC=${CC:-}" "CXX=${CXX:-}" "AR=${AR:-}" "NM=${NM:-}" "RANLIB=${RANLIB:-}"

    for var in \
        WATER_ARCH_I386 WATER_ARCH_X86_64 WATER_ARCH_ARM WATER_ARCH_AARCH64 \
        WATER_ARCH_ARM64EC WATER_ARCH_POWERPC
    do
        eval "value=\${$var:-y}"
        printf '%s=%s\n' "$var" "$value"
    done

    for var in \
        WATER_WIN16 WATER_WIN64 WATER_TESTS WATER_BUILD_ID WATER_NINJA \
        WATER_MAINTAINER_MODE WATER_SAST WATER_SILENT_RULES WATER_WERROR \
        WATER_WITH_ALSA WATER_WITH_CAPI WATER_WITH_COREAUDIO WATER_WITH_CUPS \
        WATER_WITH_DBUS WATER_WITH_FFMPEG WATER_WITH_FONTCONFIG WATER_WITH_FREETYPE \
        WATER_WITH_GETTEXT WATER_WITH_GETTEXTPO WATER_WITH_GPHOTO WATER_WITH_GNUTLS \
        WATER_WITH_GSSAPI WATER_WITH_GSTREAMER WATER_WITH_HWLOC WATER_WITH_INOTIFY \
        WATER_WITH_KRB5 WATER_WITH_MINGW WATER_WITH_NETAPI WATER_WITH_OPENCL \
        WATER_WITH_OPENGL WATER_WITH_OSS WATER_WITH_PCAP WATER_WITH_PCSCLITE \
        WATER_WITH_PTHREAD WATER_WITH_PULSE WATER_WITH_SANE WATER_WITH_SDL \
        WATER_WITH_UDEV WATER_WITH_USB WATER_WITH_V4L2 WATER_WITH_VA \
        WATER_WITH_VULKAN WATER_WITH_WAYLAND WATER_WITH_XCOMPOSITE WATER_WITH_XCURSOR \
        WATER_WITH_XFIXES WATER_WITH_XINERAMA WATER_WITH_XINPUT WATER_WITH_XINPUT2 \
        WATER_WITH_XRANDR WATER_WITH_XRENDER WATER_WITH_XSHAPE WATER_WITH_XSHM \
        WATER_WITH_XXF86VM
    do
        eval "value=\${$var:-auto}"
        printf '%s=%s\n' "$var" "$value"
    done
}

record_profile_signature()
{
    mkdir -p "$BUILD_DIR"
    tmp="$PROFILE_FILE.tmp.$$"
    profile_signature > "$tmp"
    mv -f "$tmp" "$PROFILE_FILE"
}

profile_changed()
{
    [ -f "$PROFILE_FILE" ] || return 0
    current=$(profile_signature)
    previous=$(cat "$PROFILE_FILE")
    [ "$current" != "$previous" ]
}

save_user_configure_args()
{
    mkdir -p "$BUILD_DIR"
    tmp="$CONFIGURE_USER_ARGS_FILE.tmp.$$"
    : > "$tmp"
    for arg
    do
        case "$arg" in
            *'
'*) rm -f "$tmp"; die "configure arguments may not contain newlines" ;;
        esac
        printf '%s\n' "$arg" >> "$tmp"
    done
    mv -f "$tmp" "$CONFIGURE_USER_ARGS_FILE"
}

configure_build()
{
    mkdir -p "$BUILD_DIR"

    case "$WATER_ARCHS_MODE" in
        none)
            set -- "--enable-archs=none" "$@"
            ;;
        custom)
            archs=
            for item in \
                WATER_ARCH_I386:i386 WATER_ARCH_X86_64:x86_64 WATER_ARCH_ARM:arm \
                WATER_ARCH_AARCH64:aarch64 WATER_ARCH_ARM64EC:arm64ec \
                WATER_ARCH_POWERPC:powerpc
            do
                var=${item%%:*}
                arch=${item#*:}
                eval "value=\${$var:-y}"
                case "$value" in
                    y|1)
                        if [ -n "$archs" ]; then archs="$archs,$arch"; else archs=$arch; fi
                        ;;
                esac
            done
            [ -n "$archs" ] ||
                die "WATER_ARCHS_MODE=custom requires at least one enabled architecture"
            set -- "--enable-archs=$archs" "$@"
            ;;
    esac

    value=${WATER_WIN16:-auto}
    case "$value" in
        y|1) set -- "--enable-win16=i386" "$@" ;;
        n|0) set -- "--disable-win16" "$@" ;;
    esac

    for item in \
        WATER_WIN64:win64 WATER_TESTS:tests \
        WATER_BUILD_ID:build-id WATER_NINJA:ninja \
        WATER_MAINTAINER_MODE:maintainer-mode WATER_SAST:sast \
        WATER_SILENT_RULES:silent-rules WATER_WERROR:werror
    do
        var=${item%%:*}
        option=${item#*:}
        eval "value=\${$var:-auto}"
        case "$value" in
            y|1) set -- "--enable-$option" "$@" ;;
            n|0) set -- "--disable-$option" "$@" ;;
        esac
    done

    for item in \
        WATER_WITH_ALSA:alsa WATER_WITH_CAPI:capi WATER_WITH_COREAUDIO:coreaudio \
        WATER_WITH_CUPS:cups WATER_WITH_DBUS:dbus WATER_WITH_FFMPEG:ffmpeg \
        WATER_WITH_FONTCONFIG:fontconfig WATER_WITH_FREETYPE:freetype \
        WATER_WITH_GETTEXT:gettext WATER_WITH_GETTEXTPO:gettextpo \
        WATER_WITH_GPHOTO:gphoto WATER_WITH_GNUTLS:gnutls WATER_WITH_GSSAPI:gssapi \
        WATER_WITH_GSTREAMER:gstreamer WATER_WITH_HWLOC:hwloc WATER_WITH_INOTIFY:inotify \
        WATER_WITH_KRB5:krb5 WATER_WITH_MINGW:mingw WATER_WITH_NETAPI:netapi \
        WATER_WITH_OPENCL:opencl WATER_WITH_OPENGL:opengl WATER_WITH_OSS:oss \
        WATER_WITH_PCAP:pcap WATER_WITH_PCSCLITE:pcsclite WATER_WITH_PTHREAD:pthread \
        WATER_WITH_PULSE:pulse WATER_WITH_SANE:sane WATER_WITH_SDL:sdl \
        WATER_WITH_UDEV:udev WATER_WITH_USB:usb WATER_WITH_V4L2:v4l2 WATER_WITH_VA:va \
        WATER_WITH_VULKAN:vulkan WATER_WITH_WAYLAND:wayland \
        WATER_WITH_XCOMPOSITE:xcomposite WATER_WITH_XCURSOR:xcursor \
        WATER_WITH_XFIXES:xfixes WATER_WITH_XINERAMA:xinerama \
        WATER_WITH_XINPUT:xinput WATER_WITH_XINPUT2:xinput2 WATER_WITH_XRANDR:xrandr \
        WATER_WITH_XRENDER:xrender WATER_WITH_XSHAPE:xshape WATER_WITH_XSHM:xshm \
        WATER_WITH_XXF86VM:xxf86vm
    do
        var=${item%%:*}
        option=${item#*:}
        eval "value=\${$var:-auto}"
        case "$value" in
            y|1) set -- "--with-$option" "$@" ;;
            n|0) set -- "--without-$option" "$@" ;;
        esac
    done

    case "$WATER_COMPILER_CACHE" in
        sccache|ccache) set -- "--with-compiler-cache=$WATER_COMPILER_CACHE" "$@" ;;
        none) set -- "--without-compiler-cache" "$@" ;;
    esac

    case "${WATER_SYSTEM_DLLPATH:-auto}" in
        auto|'') ;;
        *) set -- "--with-system-dllpath=$WATER_SYSTEM_DLLPATH" "$@" ;;
    esac
    case "${WATER_WINE_TOOLS:-auto}" in
        auto|'') ;;
        *) set -- "--with-wine-tools=$WATER_WINE_TOOLS" "$@" ;;
    esac
    case "${WATER_WINE64:-auto}" in
        auto|'') ;;
        *) set -- "--with-wine64=$WATER_WINE64" "$@" ;;
    esac

    printf 'WHP configure: %s\n' "$BUILD_DIR" >&2
    (
        cd "$BUILD_DIR"
        "$SOURCE_DIR/configure" "$@"
    )
    record_profile_signature
}

configure_saved()
{
    set --
    if [ -f "$CONFIGURE_USER_ARGS_FILE" ]; then
        while IFS= read -r arg || [ -n "$arg" ]; do
            set -- "$@" "$arg"
        done < "$CONFIGURE_USER_ARGS_FILE"
    fi
    configure_build "$@"
}

configure_new()
{
    save_user_configure_args "$@"
    configure_build "$@"
}

recheck_build()
{
    if [ -x "$BUILD_DIR/config.status" ]; then
        printf 'WHP configure: rechecking existing build options\n' >&2
        (
            cd "$BUILD_DIR"
            ./config.status --recheck
        )
        record_profile_signature
    else
        configure_saved
    fi
}

ensure_configured()
{
    if [ ! -f "$BUILD_DIR/Makefile" ] && [ ! -f "$BUILD_DIR/build.ninja" ]; then
        if [ ! -f "$CONFIGURE_USER_ARGS_FILE" ]; then
            save_user_configure_args
        fi
        configure_saved
    elif profile_changed; then
        printf 'WHP configure: menu/toolchain profile changed\n' >&2
        configure_saved
    elif [ "$WHP_RECONFIGURE" = 1 ]; then
        configure_saved
    elif [ -f "$BUILD_DIR/config.status" ] &&
         [ "$SOURCE_DIR/configure" -nt "$BUILD_DIR/config.status" ]; then
        recheck_build
    fi
}

run_build()
{
    jobs=$(detect_jobs)

    if [ -f "$BUILD_DIR/build.ninja" ]; then
        ninja_cmd=${NINJA_CMD:-${NINJA:-}}
        if [ -z "$ninja_cmd" ]; then
            ninja_cmd=$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)
        fi
        [ -n "$ninja_cmd" ] || die "build.ninja exists but Ninja was not found"
        "$ninja_cmd" -C "$BUILD_DIR" -j "$jobs" "$@"
        return
    fi

    make_cmd=${MAKE:-}
    if [ -z "$make_cmd" ]; then
        make_cmd=$(command -v gmake 2>/dev/null || command -v make 2>/dev/null || true)
    fi
    [ -n "$make_cmd" ] || die "make was not found"
    "$make_cmd" -C "$BUILD_DIR" -j"$jobs" "$@"
}

case "${1:-build}" in
    -h|--help|help)
        usage
        exit 0
        ;;
    menuconfig)
        run_menuconfig "$@"
        ;;
esac

load_whp_config
validate_profile
generate_configure
init_submodules
prepare_ninja
prepare_llvm_toolchain
setup_toolchain

case "${1:-build}" in
    configure)
        shift
        configure_new "$@"
        ;;
    reconfigure)
        shift
        if [ "$#" -gt 0 ]; then
            configure_new "$@"
        else
            configure_saved
        fi
        ;;
    build|incremental)
        if [ "$#" -gt 0 ]; then shift; fi
        ensure_configured
        run_build "$@"
        ;;
    *)
        ensure_configured
        run_build "$@"
        ;;
esac
