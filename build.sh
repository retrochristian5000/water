#!/bin/sh

set -eu

SOURCE_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
BUILD_DIR=${WHP_BUILD_DIR:-"$SOURCE_DIR/build"}
LLVM_SOURCE_DIR=${WHP_LLVM_SOURCE_DIR:-"$SOURCE_DIR/toolchains/llvm-project"}
LLVM_BOOTSTRAP_DIR=${WHP_LLVM_BUILD_DIR:-"$BUILD_DIR/llvm-bootstrap"}
LLVM_LIBCXX_RUNTIME_DIR=${WHP_LIBCXX_RUNTIME_DIR:-"$BUILD_DIR/llvm-libcxx-pe"}
BASH_SOURCE_DIR=${WHP_BASH_SOURCE_DIR:-"$SOURCE_DIR/toolchains/bash"}
BASH_STAGE_DIR=${WHP_BASH_STAGE_DIR:-"$BUILD_DIR/bash-source"}
BASH_EFFECTIVE_SOURCE_DIR=$BASH_SOURCE_DIR
BASH_BOOTSTRAP_DIR=${WHP_BASH_BUILD_DIR:-"$BUILD_DIR/bash-bootstrap"}
LLVM_LINK_JOBS=${WHP_LLVM_LINK_JOBS:-2}
WHP_GIT_UPDATE=${WHP_GIT_UPDATE:-1}
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
BASH_BOOTSTRAP_STATE_FILE="$BASH_BOOTSTRAP_DIR/.whp-state"
LLVM_BOOTSTRAP_RECIPE=6
LLVM_LIBCXX_RECIPE=7
BASH_BOOTSTRAP_RECIPE=2
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

case "$WHP_GIT_UPDATE" in
    0|1) ;;
    *) die "WHP_GIT_UPDATE must be 0 or 1" ;;
esac

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
  WATER_KEEP_GOING      Continue independent work after errors: y or n (default: y)
  WHP_LLVM_SOURCE_DIR   LLVM source tree (default: ./toolchains/llvm-project)
  WHP_LLVM_BUILD_DIR    Water LLVM bootstrap directory (default: ./build/llvm-bootstrap)
  WHP_LIBCXX_RUNTIME_DIR LLVM libc++ PE runtime cache (default: ./build/llvm-libcxx-pe)
  WHP_BASH_SOURCE_DIR   WHP Bash source tree (default: ./toolchains/bash)
  WHP_BASH_BUILD_DIR    Cached WHP Bash host-tool build (default: ./build/bash-bootstrap)
  WHP_BASH_STAGE_DIR    Immutable pinned Bash source snapshot (default: ./build/bash-source)
  WHP_BASH_CMD          Explicit Bash executable for bootstrap orchestration
  WHP_LLVM_LINK_JOBS    Concurrent LLVM link jobs (default: 2)
  WHP_LLVM_PREFIX       Built/installed LLVM prefix to prefer
  WATER_LLVM_LINKER     Host linker policy: auto, lld, or system (default: auto)
  WATER_LIBCXX          PE libc++ provider: llvm or legacy (default: llvm)
  WHP_LLVM_BOOTSTRAP_CC Stage-0 C compiler (default: prefer clang)
  WHP_LLVM_BOOTSTRAP_CXX Stage-0 C++ compiler (default: prefer clang++)
  NINJA_CMD              Explicit Ninja executable shared by LLVM and Water
  BOOTSTRAP_NINJA        Pinned WHP Ninja policy: auto, y, or n
  WATER_BASH_BOOTSTRAP   Pinned WHP Bash policy: auto, y, or n
  WHP_GIT_UPDATE        Rebase Water onto its configured upstream: 1 or 0 (default: 1)
  WHP_SUBMODULES        Initialize pinned submodules: 1 or 0 (default: 1)
  WHP_RECONFIGURE       Re-run configure before building: 1 or 0 (default: 0)
  AUTOCONF              Autoconf program used to generate ./configure
  WATER_WITH_MINGW      PE compiler policy: auto, clang, llvm-mingw, y, or n
                         (auto prefers Water's selected LLVM clang)

Run ./build.sh menuconfig to edit the persistent .whpconfig profile.
Explicit environment variables and explicit configure arguments override menu defaults.
CC/CXX/AR/NM/RANLIB/LD and linker flags remain authoritative when explicitly set.
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
    WATER_LLVM_LINKER=${WATER_LLVM_LINKER:-auto}
    WATER_LIBCXX=${WATER_LIBCXX:-llvm}
    WATER_BASH_BOOTSTRAP=${WATER_BASH_BOOTSTRAP:-auto}
    WATER_COMPILER_CACHE=${WATER_COMPILER_CACHE:-auto}
    WATER_KEEP_GOING=${WATER_KEEP_GOING:-y}
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
    case "$WATER_LLVM_LINKER" in
        auto|lld|system) ;;
        *) die "WATER_LLVM_LINKER must be auto, lld, or system" ;;
    esac
    case "$WATER_LIBCXX" in
        llvm|legacy) ;;
        *) die "WATER_LIBCXX must be llvm or legacy" ;;
    esac
    case "$WATER_BASH_BOOTSTRAP" in
        auto|y|n|0|1) ;;
        *) die "WATER_BASH_BOOTSTRAP must be auto, y, or n" ;;
    esac
    case "$LLVM_LINK_JOBS" in
        ''|*[!0-9]*|0) die "WHP_LLVM_LINK_JOBS must be a positive integer" ;;
    esac
    case "$WATER_COMPILER_CACHE" in
        auto|sccache|ccache|none) ;;
        *) die "WATER_COMPILER_CACHE must be auto, sccache, ccache, or none" ;;
    esac
    case "$WATER_KEEP_GOING" in
        y|n|0|1) ;;
        *) die "WATER_KEEP_GOING must be y or n" ;;
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
        WATER_WITH_KRB5 WATER_WITH_NETAPI WATER_WITH_OPENCL \
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

    case "${WATER_WITH_MINGW:-auto}" in
        auto|clang|llvm-mingw|y|n|0|1|*/clang) ;;
        *) die "WATER_WITH_MINGW must be auto, clang, llvm-mingw, y, n, or a path ending in /clang" ;;
    esac
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

update_repository()
{
    [ "$WHP_GIT_UPDATE" = 1 ] || return 0
    [ -d "$SOURCE_DIR/.git" ] || return 0

    command -v git >/dev/null 2>&1 ||
        die "git is required to update the Water source tree"

    printf 'WHP source update: git pull --rebase --recurse-submodules=no\n' >&2
    git -C "$SOURCE_DIR" pull --rebase --recurse-submodules=no
}

init_submodules()
{
    [ "$WHP_SUBMODULES" = 1 ] || return 0
    command -v git >/dev/null 2>&1 || die "git is required to initialize Water submodules"

    # Ninja initializes its own source lazily only when its bootstrap path
    # is selected. Keep unconditional submodule work to build-required modules.
    whp_submodules="libs/fluidsynth"
    if [ "$LLVM_SOURCE_DIR" = "$SOURCE_DIR/toolchains/llvm-project" ]; then
        whp_submodules="$whp_submodules toolchains/llvm-project"
    fi

    # WHP Bash is staged from its pinned git object by prepare_bash_toolchain().
    # Do not checkout/update the mutable Bash worktree here: local or generated
    # files in that submodule must never block an incremental Water build.
    git -C "$SOURCE_DIR" submodule sync --recursive
    git -C "$SOURCE_DIR" submodule update --init --recursive $whp_submodules
    unset whp_submodules
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

find_existing_ninja()
{
    if [ -n "${NINJA_CMD:-}" ]; then
        printf '%s\n' "$NINJA_CMD"
        return 0
    fi
    if [ -n "${NINJA:-}" ]; then
        printf '%s\n' "$NINJA"
        return 0
    fi

    whp_ninja_system=$(uname -s 2>/dev/null | tr '[:upper:]' '[:lower:]')
    whp_ninja_machine=$(uname -m 2>/dev/null | tr '[:upper:]' '[:lower:]')
    whp_ninja_dir="$BUILD_DIR/../.whp-host-tools/ninja-${whp_ninja_system}-${whp_ninja_machine}"

    for whp_ninja_candidate in "$whp_ninja_dir/ninja" "$whp_ninja_dir/ninja.exe"
    do
        if [ -x "$whp_ninja_candidate" ]; then
            printf '%s\n' "$whp_ninja_candidate"
            unset whp_ninja_system whp_ninja_machine whp_ninja_dir whp_ninja_candidate
            return 0
        fi
    done

    whp_ninja_candidate=$(command -v ninja 2>/dev/null || command -v ninja-build 2>/dev/null || true)
    printf '%s\n' "$whp_ninja_candidate"
    unset whp_ninja_system whp_ninja_machine whp_ninja_dir whp_ninja_candidate
}

darwin_sdkroot()
{
    case "$(uname -s 2>/dev/null || true)" in
        Darwin) ;;
        *) return 0 ;;
    esac

    if [ -n "${SDKROOT:-}" ] && [ -d "$SDKROOT" ]; then
        sdkroot=$(cd "$SDKROOT" 2>/dev/null && pwd -P) ||
            die "could not canonicalize macOS SDK root '$SDKROOT'"
        printf '%s\n' "$sdkroot"
        return 0
    fi

    xcrun_cmd=$(command -v xcrun 2>/dev/null || true)
    [ -n "$xcrun_cmd" ] ||
        die "xcrun is required to resolve the macOS SDK name"

    if [ -n "${SDKROOT:-}" ]; then
        sdkroot=$("$xcrun_cmd" --sdk "$SDKROOT" --show-sdk-path 2>/dev/null || true)
    else
        sdkroot=$("$xcrun_cmd" --sdk macosx --show-sdk-path 2>/dev/null || true)
    fi

    [ -n "$sdkroot" ] && [ -d "$sdkroot" ] ||
        die "could not resolve a usable macOS SDK (SDKROOT=${SDKROOT:-auto})"
    printf '%s\n' "$sdkroot"
}

select_llvm_lld_backends()
{
    case "$WATER_LLVM_LINKER:$(uname -s 2>/dev/null || true)" in
        lld:Darwin)
            printf '%s\n' 'COFF;MinGW;MachO'
            ;;
        lld:Linux|lld:FreeBSD|lld:NetBSD|lld:OpenBSD|lld:DragonFly|lld:SunOS|lld:Haiku)
            printf '%s\n' 'COFF;MinGW;ELF'
            ;;
        auto:Linux|auto:FreeBSD|auto:NetBSD|auto:OpenBSD|auto:DragonFly|auto:SunOS|auto:Haiku)
            printf '%s\n' 'COFF;MinGW;ELF'
            ;;
        *)
            # Darwin auto/system deliberately avoid Mach-O LLD.  Water can use
            # Apple ld without making the optional Mach-O backend a bootstrap
            # dependency.  Explicit WATER_LLVM_LINKER=lld opts into Mach-O.
            printf '%s\n' 'COFF;MinGW'
            ;;
    esac
}

host_lld_path()
{
    whp_lld_bin=$1
    case "$(uname -s 2>/dev/null || true)" in
        Darwin) whp_lld_name=ld64.lld ;;
        Linux|FreeBSD|NetBSD|OpenBSD|DragonFly|SunOS|Haiku) whp_lld_name=ld.lld ;;
        *)
            unset whp_lld_bin
            return 1
            ;;
    esac

    whp_lld_path="$whp_lld_bin/$whp_lld_name"
    if [ -x "$whp_lld_path" ]; then
        printf '%s\n' "$whp_lld_path"
        unset whp_lld_bin whp_lld_name whp_lld_path
        return 0
    fi

    unset whp_lld_bin whp_lld_name whp_lld_path
    return 1
}

darwin_arm64e_requested()
{
    whp_arm64e_compiler=${1:-}

    case "$(uname -s 2>/dev/null || true)" in
        Darwin) ;;
        *)
            unset whp_arm64e_compiler
            return 1
            ;;
    esac

    case " ${CFLAGS:-} ${CXXFLAGS:-} ${LDFLAGS:-} " in
        *" -arch arm64e "*|*" -target arm64e-"*|*" --target=arm64e-"*)
            unset whp_arm64e_compiler
            return 0
            ;;
    esac

    if [ -n "$whp_arm64e_compiler" ]; then
        whp_arm64e_target=$("$whp_arm64e_compiler" -print-target-triple 2>/dev/null || true)
        case "$whp_arm64e_target" in
            arm64e-*|aarch64e-*)
                unset whp_arm64e_compiler whp_arm64e_target
                return 0
                ;;
        esac
        unset whp_arm64e_target
    fi

    unset whp_arm64e_compiler
    return 1
}

probe_lld_linker()
{
    whp_probe_compiler=$1
    whp_probe_linker=$2
    whp_probe_language=$3
    whp_probe_sdkroot=${4:-}
    whp_probe_output=$(mktemp "${TMPDIR:-/tmp}/whp-water-linker-probe.XXXXXX") ||
        return 1
    whp_probe_linker_dir=$(dirname -- "$whp_probe_linker")

    rm -f "$whp_probe_output"
    if [ -n "$whp_probe_sdkroot" ]; then
        if printf 'int main(void) { return 0; }\n' |
            PATH="$whp_probe_linker_dir:$PATH" "$whp_probe_compiler" \
                -fuse-ld=lld -isysroot "$whp_probe_sdkroot" \
                -x "$whp_probe_language" - -o "$whp_probe_output" >/dev/null 2>&1
        then
            rm -f "$whp_probe_output"
            unset whp_probe_compiler whp_probe_linker whp_probe_language \
                whp_probe_sdkroot whp_probe_output whp_probe_linker_dir
            return 0
        fi
    elif printf 'int main(void) { return 0; }\n' |
        PATH="$whp_probe_linker_dir:$PATH" "$whp_probe_compiler" \
            -fuse-ld=lld -x "$whp_probe_language" - -o "$whp_probe_output" >/dev/null 2>&1
    then
        rm -f "$whp_probe_output"
        unset whp_probe_compiler whp_probe_linker whp_probe_language \
            whp_probe_sdkroot whp_probe_output whp_probe_linker_dir
        return 0
    fi

    rm -f "$whp_probe_output"
    unset whp_probe_compiler whp_probe_linker whp_probe_language \
        whp_probe_sdkroot whp_probe_output whp_probe_linker_dir
    return 1
}

probe_darwin_objc_lld()
{
    whp_objc_compiler=$1
    whp_objc_linker=$2
    whp_objc_sdkroot=$3
    whp_objc_object=$(mktemp "${TMPDIR:-/tmp}/whp-water-objc-probe.XXXXXX") ||
        return 1
    whp_objc_output=$(mktemp "${TMPDIR:-/tmp}/whp-water-objc-link.XXXXXX") ||
    {
        rm -f "$whp_objc_object"
        unset whp_objc_compiler whp_objc_linker whp_objc_sdkroot whp_objc_object
        return 1
    }
    whp_objc_linker_dir=$(dirname -- "$whp_objc_linker")

    rm -f "$whp_objc_object" "$whp_objc_output"
    if printf '%s\n' \
        '#import <Foundation/Foundation.h>' \
        'int main(void)' \
        '{' \
        '    @autoreleasepool { return [NSObject class] ? 0 : 1; }' \
        '}' |
        "$whp_objc_compiler" -isysroot "$whp_objc_sdkroot" \
            -x objective-c -c -o "$whp_objc_object" - >/dev/null 2>&1 &&
       PATH="$whp_objc_linker_dir:$PATH" "$whp_objc_compiler" \
            -fuse-ld=lld -isysroot "$whp_objc_sdkroot" \
            "$whp_objc_object" -framework Foundation \
            -o "$whp_objc_output" >/dev/null 2>&1
    then
        rm -f "$whp_objc_object" "$whp_objc_output"
        unset whp_objc_compiler whp_objc_linker whp_objc_sdkroot \
            whp_objc_object whp_objc_output whp_objc_linker_dir
        return 0
    fi

    rm -f "$whp_objc_object" "$whp_objc_output"
    unset whp_objc_compiler whp_objc_linker whp_objc_sdkroot \
        whp_objc_object whp_objc_output whp_objc_linker_dir
    return 1
}

probe_water_host_lld()
{
    whp_host_probe_compiler=$1
    whp_host_probe_linker=$2
    whp_host_probe_sdkroot=${3:-}

    probe_lld_linker "$whp_host_probe_compiler" "$whp_host_probe_linker" c "$whp_host_probe_sdkroot" ||
    {
        unset whp_host_probe_compiler whp_host_probe_linker whp_host_probe_sdkroot
        return 1
    }

    case "$(uname -s 2>/dev/null || true)" in
        Darwin)
            probe_darwin_objc_lld "$whp_host_probe_compiler" "$whp_host_probe_linker" \
                "$whp_host_probe_sdkroot" ||
            {
                unset whp_host_probe_compiler whp_host_probe_linker whp_host_probe_sdkroot
                return 1
            }
            ;;
    esac

    unset whp_host_probe_compiler whp_host_probe_linker whp_host_probe_sdkroot
    return 0
}

linker_flag_is_explicit()
{
    case " ${LDFLAGS:-} " in
        *" -fuse-ld="*|*" --ld-path="*|*" -Wl,-ld_classic"*|*" -Wl,-ld_new"*)
            return 0
            ;;
    esac
    return 1
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

find_llvm_bootstrap_compiler()
{
    whp_bootstrap_explicit=$1
    whp_bootstrap_fallback=$2
    whp_bootstrap_path=

    if [ -n "$whp_bootstrap_explicit" ]; then
        case "$whp_bootstrap_explicit" in
            */*)
                [ -x "$whp_bootstrap_explicit" ] ||
                    die "LLVM bootstrap compiler is not executable: $whp_bootstrap_explicit"
                whp_bootstrap_path=$whp_bootstrap_explicit
                ;;
            *)
                whp_bootstrap_path=$(command -v "$whp_bootstrap_explicit" 2>/dev/null || true)
                [ -n "$whp_bootstrap_path" ] ||
                    die "LLVM bootstrap compiler was not found: $whp_bootstrap_explicit"
                ;;
        esac
    else
        whp_bootstrap_path=$(command -v "$whp_bootstrap_fallback" 2>/dev/null || true)
    fi

    printf '%s\n' "$whp_bootstrap_path"
    unset whp_bootstrap_explicit whp_bootstrap_fallback whp_bootstrap_path
}

llvm_bootstrap_config_signature()
{
    llvm_targets_sig=$(select_llvm_targets)
    llvm_cache_sig=$(select_llvm_cache)
    llvm_sdkroot_sig=$(darwin_sdkroot)
    llvm_stage0_cc_sig=$(find_llvm_bootstrap_compiler "${WHP_LLVM_BOOTSTRAP_CC:-}" clang)
    llvm_stage0_cxx_sig=$(find_llvm_bootstrap_compiler "${WHP_LLVM_BOOTSTRAP_CXX:-}" clang++)
    llvm_lld_backends_sig=$(select_llvm_lld_backends)
    llvm_host_lld_sig=none
    case "$WATER_LLVM_LINKER:$(uname -s 2>/dev/null || true)" in
        lld:*|auto:Linux|auto:FreeBSD|auto:NetBSD|auto:OpenBSD|auto:DragonFly|auto:SunOS|auto:Haiku)
            llvm_host_lld_sig=$(host_lld_path "$LLVM_BOOTSTRAP_DIR/bin" || true)
            [ -n "$llvm_host_lld_sig" ] || llvm_host_lld_sig=none
            ;;
    esac

    printf '%s\n' \
        "LLVM_BOOTSTRAP_RECIPE=$LLVM_BOOTSTRAP_RECIPE" \
        "WATER_LLVM_BUILD_TYPE=$WATER_LLVM_BUILD_TYPE" \
        "WATER_LLVM_ASSERTIONS=$WATER_LLVM_ASSERTIONS" \
        "WATER_LLVM_LEAN=$WATER_LLVM_LEAN" \
        "WATER_LLVM_PCH=$WATER_LLVM_PCH" \
        "WATER_LLVM_LINKER=$WATER_LLVM_LINKER" \
        "WHP_LLVM_LINK_JOBS=$LLVM_LINK_JOBS" \
        "WATER_COMPILER_CACHE=$WATER_COMPILER_CACHE" \
        "LLVM_TARGETS=$llvm_targets_sig" \
        "LLVM_LLD_BACKENDS=$llvm_lld_backends_sig" \
        "LLVM_HOST_LLD=$llvm_host_lld_sig" \
        "LLVM_CACHE=$llvm_cache_sig" \
        "LLVM_SDKROOT=$llvm_sdkroot_sig" \
        "LLVM_STAGE0_CC=$llvm_stage0_cc_sig" \
        "LLVM_STAGE0_CXX=$llvm_stage0_cxx_sig" \
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
    [ -x "$LLVM_BOOTSTRAP_DIR/bin/llvm-dlltool" ] || return 0
    [ -x "$LLVM_BOOTSTRAP_DIR/bin/llvm-rc" ] || return 0
    [ -f "$LLVM_BOOTSTRAP_STATE_FILE" ] || return 0

    llvm_source_state_signature >/dev/null 2>&1 || return 0

    current=$(llvm_bootstrap_state_signature)
    previous=$(cat "$LLVM_BOOTSTRAP_STATE_FILE")
    [ "$current" != "$previous" ]
}

llvm_bootstrap_has_target()
{
    whp_target=$1

    if [ -f "$LLVM_BOOTSTRAP_DIR/build.ninja" ]; then
        ninja_cmd=$(sed -n 's/^CMAKE_MAKE_PROGRAM:[^=]*=//p' "$LLVM_BOOTSTRAP_DIR/CMakeCache.txt" 2>/dev/null | sed -n '1p')
        if [ -z "$ninja_cmd" ] || [ ! -x "$ninja_cmd" ]; then
            ninja_cmd=$(find_existing_ninja)
        fi
        [ -n "$ninja_cmd" ] || { unset whp_target; return 1; }
        if "$ninja_cmd" -C "$LLVM_BOOTSTRAP_DIR" -t targets all 2>/dev/null |
           awk -F: -v wanted="$whp_target" '$1 == wanted { found = 1 } END { exit !found }'; then
            unset whp_target
            return 0
        fi
        unset whp_target
        return 1
    fi

    if [ -f "$LLVM_BOOTSTRAP_DIR/Makefile" ]; then
        if grep -Eq "^${whp_target}([[:space:]]*:|:)" "$LLVM_BOOTSTRAP_DIR/Makefile"; then
            unset whp_target
            return 0
        fi
        unset whp_target
        return 1
    fi

    unset whp_target
    return 1
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
    llvm_stage0_cc=$(find_llvm_bootstrap_compiler "${WHP_LLVM_BOOTSTRAP_CC:-}" clang)
    llvm_stage0_cxx=$(find_llvm_bootstrap_compiler "${WHP_LLVM_BOOTSTRAP_CXX:-}" clang++)
    llvm_lld_backends=$(select_llvm_lld_backends)
    llvm_sdkroot=$(darwin_sdkroot)
    llvm_use_linker=
    llvm_bootstrap_linker=system
    llvm_bootstrap_saved_path=$PATH

    mkdir -p "$LLVM_BOOTSTRAP_DIR"

    if [ "$WATER_LLVM_LINKER" != system ]; then
        llvm_previous_lld=$(host_lld_path "$LLVM_BOOTSTRAP_DIR/bin" || true)
        if [ -n "$llvm_previous_lld" ] && \
           ! darwin_arm64e_requested "$llvm_stage0_cxx"
        then
            case "$(uname -s 2>/dev/null || true)" in
                Darwin)
                    if [ "$WATER_LLVM_LINKER" = lld ] && \
                       probe_darwin_objc_lld "$llvm_stage0_cxx" "$llvm_previous_lld" "$llvm_sdkroot"
                    then
                        llvm_use_linker=lld
                    fi
                    ;;
                *)
                    if probe_lld_linker "$llvm_stage0_cxx" "$llvm_previous_lld" c++ "$llvm_sdkroot"
                    then
                        llvm_use_linker=lld
                    fi
                    ;;
            esac
            if [ "$llvm_use_linker" = lld ]; then
                llvm_bootstrap_linker=$llvm_previous_lld
                PATH="$(dirname -- "$llvm_previous_lld"):$PATH"
                export PATH
            fi
        fi
        unset llvm_previous_lld
    fi

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
        "-DLLVM_USE_LINKER=$llvm_use_linker" \
        -DLLVM_APPEND_VC_REV=OFF \
        -DLLVM_ENABLE_LTO=OFF \
        -DLLVM_ENABLE_FATLTO=OFF \
        -DLLVM_BUILD_INSTRUMENTED=OFF \
        -DLLVM_ENABLE_MODULES=OFF \
        -DLLVM_ENABLE_PLUGINS=OFF

    if [ -n "$llvm_stage0_cc" ]; then
        set -- "$@" "-DCMAKE_C_COMPILER=$llvm_stage0_cc"
        printf 'WHP LLVM stage-0 C compiler: %s\n' "$llvm_stage0_cc" >&2
    fi
    if [ -n "$llvm_stage0_cxx" ]; then
        set -- "$@" "-DCMAKE_CXX_COMPILER=$llvm_stage0_cxx"
        printf 'WHP LLVM stage-0 C++ compiler: %s\n' "$llvm_stage0_cxx" >&2
    fi

    if [ -n "$llvm_sdkroot" ]; then
        set -- "$@" "-DCMAKE_OSX_SYSROOT=$llvm_sdkroot"
        printf 'WHP LLVM macOS SDK: %s\n' "$llvm_sdkroot" >&2
    fi
    printf 'WHP LLVM bootstrap linker: %s\n' "$llvm_bootstrap_linker" >&2

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
        ninja_cmd=$(find_existing_ninja)
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
            for whp_required_target in clang lld llvm-ar llvm-dlltool llvm-rc llvm-nm llvm-ranlib llvm-strip
            do
                if ! llvm_bootstrap_has_target "$whp_required_target"; then
                    printf 'WHP LLVM CMake: cached target %s is missing; regenerating\n' "$whp_required_target" >&2
                    llvm_configure=1
                    break
                fi
            done
            unset whp_required_target

            llvm_cached_linker=$(sed -n 's/^LLVM_USE_LINKER:[^=]*=//p' "$LLVM_BOOTSTRAP_DIR/CMakeCache.txt" |
                sed -n '1p')
            if [ "$llvm_cached_linker" != "$llvm_use_linker" ]; then
                printf 'WHP LLVM CMake: linker changed (%s -> %s); regenerating\n' \
                    "${llvm_cached_linker:-system}" "${llvm_use_linker:-system}" >&2
                llvm_configure=1
            fi
            unset llvm_cached_linker
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

    for whp_required_target in clang lld llvm-ar llvm-dlltool llvm-rc llvm-nm llvm-ranlib llvm-strip
    do
        llvm_bootstrap_has_target "$whp_required_target" ||
            die "LLVM bootstrap target '$whp_required_target' is missing after CMake generation"
    done
    unset whp_required_target

    jobs=$(detect_jobs)
    printf 'WHP LLVM bootstrap: incremental %s\n' "$LLVM_BOOTSTRAP_DIR" >&2
    llvm_generator=$(sed -n 's/^CMAKE_GENERATOR:INTERNAL=//p' "$LLVM_BOOTSTRAP_DIR/CMakeCache.txt" | sed -n '1p')
    if [ "$WATER_KEEP_GOING" = y ] || [ "$WATER_KEEP_GOING" = 1 ]; then
        case "$llvm_generator" in
            Ninja*)
                "$cmake_cmd" --build "$LLVM_BOOTSTRAP_DIR" --parallel "$jobs" \
                    --target clang lld llvm-ar llvm-dlltool llvm-rc llvm-nm llvm-ranlib llvm-strip -- -k 0
                ;;
            *Makefiles*)
                "$cmake_cmd" --build "$LLVM_BOOTSTRAP_DIR" --parallel "$jobs" \
                    --target clang lld llvm-ar llvm-dlltool llvm-rc llvm-nm llvm-ranlib llvm-strip -- -k
                ;;
            *)
                "$cmake_cmd" --build "$LLVM_BOOTSTRAP_DIR" --parallel "$jobs" \
                    --target clang lld llvm-ar llvm-dlltool llvm-rc llvm-nm llvm-ranlib llvm-strip
                ;;
        esac
    else
        "$cmake_cmd" --build "$LLVM_BOOTSTRAP_DIR" --parallel "$jobs" \
            --target clang lld llvm-ar llvm-dlltool llvm-rc llvm-nm llvm-ranlib llvm-strip
    fi
    unset llvm_generator
    PATH=$llvm_bootstrap_saved_path
    export PATH
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
            WHP_DARWIN_SDKROOT=$(darwin_sdkroot)
            if [ -n "$WHP_DARWIN_SDKROOT" ]; then
                SDKROOT=$WHP_DARWIN_SDKROOT
                export SDKROOT
                printf 'WHP macOS SDK: %s\n' "$WHP_DARWIN_SDKROOT" >&2
            fi

            cc_has_assert=1
            cxx_has_assert=1
            if [ "$whp_auto_cc" = 1 ]; then
                compiler_has_assert_h "$CC" c || cc_has_assert=0
            fi
            if [ "$whp_auto_cxx" = 1 ]; then
                compiler_has_assert_h "$CXX" c++ || cxx_has_assert=0
            fi

            if [ "$cc_has_assert" = 0 ] || [ "$cxx_has_assert" = 0 ]; then
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

    WHP_HOST_LINKER=system
    whp_try_host_lld=0
    case "$WATER_LLVM_LINKER:$(uname -s 2>/dev/null || true)" in
        lld:*|auto:Linux|auto:FreeBSD|auto:NetBSD|auto:OpenBSD|auto:DragonFly|auto:SunOS|auto:Haiku)
            whp_try_host_lld=1
            ;;
    esac

    if [ "$whp_try_host_lld" = 1 ] && [ -n "$LLVM_BIN" ] && \
       [ -z "${LD:-}" ] && ! linker_flag_is_explicit
    then
        whp_host_lld=$(host_lld_path "$LLVM_BIN" || true)
        if [ -n "$whp_host_lld" ]; then
            if darwin_arm64e_requested "$CC"; then
                if [ "$WATER_LLVM_LINKER" = lld ]; then
                    die "WATER_LLVM_LINKER=lld is unsafe for arm64e until WHP Mach-O LLD supports authenticated relocations"
                fi
                printf 'WHP host linker: system (arm64e requires Apple ld)\n' >&2
            elif probe_water_host_lld "$CC" "$whp_host_lld" "$WHP_DARWIN_SDKROOT"; then
                LD=$whp_host_lld
                whp_host_lld_dir=$(dirname -- "$whp_host_lld")
                PATH="$whp_host_lld_dir:$PATH"
                case " ${LDFLAGS:-} " in
                    *" -fuse-ld=lld "*) ;;
                    *) LDFLAGS="${LDFLAGS:+$LDFLAGS }-fuse-ld=lld" ;;
                esac
                WHP_HOST_LINKER=$whp_host_lld
                export LD LDFLAGS PATH
                unset whp_host_lld_dir
            elif [ "$WATER_LLVM_LINKER" = lld ]; then
                die "selected LLVM host linker failed the Water host link probes: $whp_host_lld"
            fi
        elif [ "$WATER_LLVM_LINKER" = lld ]; then
            die "WATER_LLVM_LINKER=lld requested, but the selected LLVM toolchain has no host-format LLD"
        fi
        unset whp_host_lld
    elif [ -n "${LD:-}" ]; then
        WHP_HOST_LINKER=$LD
    elif linker_flag_is_explicit; then
        WHP_HOST_LINKER="driver flags"
    fi
    unset whp_try_host_lld
    export WHP_HOST_LINKER
    printf 'WHP host linker: %s\n' "$WHP_HOST_LINKER" >&2

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

llvm_libcxx_source_id()
{
    whp_libcxx_source_id=$(git -C "$LLVM_SOURCE_DIR" rev-parse HEAD 2>/dev/null || true)
    if [ -z "$whp_libcxx_source_id" ]; then
        whp_libcxx_source_id=$(cksum "$LLVM_SOURCE_DIR/libcxx/include/__config" \
            "$LLVM_SOURCE_DIR/libcxx/CMakeLists.txt" 2>/dev/null |
            awk '{ printf "%s:%s;", $1, $2 }')
    fi
    printf '%s\n' "$whp_libcxx_source_id"
    unset whp_libcxx_source_id
}

selected_libcxx_archs()
{
    whp_libcxx_archs=
    whp_libcxx_archs_found=0

    if [ "$WHP_CONFIGURE_ARCHS_SET" = 1 ]; then
        whp_libcxx_archs=$WHP_CONFIGURE_ARCHS
        whp_libcxx_archs_found=1
    elif [ -f "$CONFIGURE_USER_ARGS_FILE" ]; then
        while IFS= read -r whp_libcxx_arg || [ -n "$whp_libcxx_arg" ]; do
            case "$whp_libcxx_arg" in
                --enable-archs=*)
                    whp_libcxx_archs=${whp_libcxx_arg#--enable-archs=}
                    whp_libcxx_archs_found=1
                    ;;
                --disable-archs)
                    whp_libcxx_archs=none
                    whp_libcxx_archs_found=1
                    ;;
            esac
        done < "$CONFIGURE_USER_ARGS_FILE"
        unset whp_libcxx_arg
    fi

    if [ "$whp_libcxx_archs_found" = 0 ]; then
        case "$WATER_ARCHS_MODE" in
            none)
                whp_libcxx_archs=none
                ;;
            custom)
                for whp_libcxx_item in \
                    WATER_ARCH_I386:i386 WATER_ARCH_X86_64:x86_64 WATER_ARCH_ARM:arm \
                    WATER_ARCH_AARCH64:aarch64 WATER_ARCH_ARM64EC:arm64ec \
                    WATER_ARCH_POWERPC:powerpc
                do
                    whp_libcxx_var=${whp_libcxx_item%%:*}
                    whp_libcxx_arch=${whp_libcxx_item#*:}
                    eval "whp_libcxx_value=\${$whp_libcxx_var:-y}"
                    case "$whp_libcxx_value" in
                        y|1)
                            if [ -n "$whp_libcxx_archs" ]; then
                                whp_libcxx_archs="$whp_libcxx_archs,$whp_libcxx_arch"
                            else
                                whp_libcxx_archs=$whp_libcxx_arch
                            fi
                            ;;
                    esac
                done
                unset whp_libcxx_item whp_libcxx_var whp_libcxx_arch whp_libcxx_value
                ;;
            auto)
                case "$(uname -m 2>/dev/null || true):$(uname -s 2>/dev/null || true)" in
                    x86_64:Darwin|amd64:Darwin) whp_libcxx_archs=x86_64 ;;
                    x86_64:*|amd64:*)            whp_libcxx_archs=i386,x86_64 ;;
                    arm64:*|aarch64:*)           whp_libcxx_archs=aarch64 ;;
                    i?86:*)                      whp_libcxx_archs=i386 ;;
                    armv7*:*)                    whp_libcxx_archs=arm ;;
                    ppc*:*|powerpc*:*)           whp_libcxx_archs=powerpc ;;
                    *)                           whp_libcxx_archs=none ;;
                esac
                ;;
        esac
    fi

    case "$whp_libcxx_archs" in
        ""|none|no)
            ;;
        *)
            printf '%s\n' "$whp_libcxx_archs" | tr ',' ' '
            ;;
    esac
    unset whp_libcxx_archs whp_libcxx_archs_found
}

selected_llvm_libcxx_archs()
{
    whp_libcxx_selected=$(selected_libcxx_archs)
    whp_libcxx_prepare=
    for whp_libcxx_arch in $whp_libcxx_selected
    do
        case "$whp_libcxx_arch" in
            i386|x86_64|aarch64|arm64ec)
                case " $whp_libcxx_prepare " in
                    *" $whp_libcxx_arch "*) ;;
                    *) whp_libcxx_prepare="$whp_libcxx_prepare $whp_libcxx_arch" ;;
                esac
                if [ "$whp_libcxx_arch" = arm64ec ]; then
                    case " $whp_libcxx_prepare " in
                        *" x86_64 "*) ;;
                        *) whp_libcxx_prepare="$whp_libcxx_prepare x86_64" ;;
                    esac
                fi
                ;;
        esac
    done
    printf '%s\n' "$whp_libcxx_prepare"
    unset whp_libcxx_selected whp_libcxx_prepare whp_libcxx_arch
}

bash_pinned_revision()
{
    [ "$BASH_SOURCE_DIR" = "$SOURCE_DIR/toolchains/bash" ] || return 1
    [ -d "$SOURCE_DIR/.git" ] || return 1
    git -C "$SOURCE_DIR" ls-tree HEAD -- toolchains/bash 2>/dev/null |
        awk '$2 == "commit" { print $3; exit }'
}

stage_pinned_bash_source()
{
    BASH_EFFECTIVE_SOURCE_DIR=$BASH_SOURCE_DIR

    [ "$BASH_SOURCE_DIR" = "$SOURCE_DIR/toolchains/bash" ] || {
        export BASH_EFFECTIVE_SOURCE_DIR
        return 0
    }
    [ -d "$SOURCE_DIR/.git" ] || {
        export BASH_EFFECTIVE_SOURCE_DIR
        return 0
    }

    whp_bash_expected=$(bash_pinned_revision)
    [ -n "$whp_bash_expected" ] || {
        printf 'WHP Bash: Water gitlink is unavailable; using source path directly\n' >&2
        export BASH_EFFECTIVE_SOURCE_DIR
        unset whp_bash_expected
        return 0
    }

    if ! git -C "$BASH_SOURCE_DIR" rev-parse --git-dir >/dev/null 2>&1; then
        if [ "$WHP_SUBMODULES" != 1 ]; then
            printf 'WHP Bash: submodule is not initialized and WHP_SUBMODULES=0\n' >&2
            unset whp_bash_expected
            return 1
        fi
        if ! git -C "$SOURCE_DIR" submodule update --init --depth 1 --no-fetch toolchains/bash 2>/dev/null &&
           ! git -C "$SOURCE_DIR" submodule update --init --depth 1 toolchains/bash; then
            printf 'WHP Bash: failed to initialize pinned submodule source\n' >&2
            unset whp_bash_expected
            return 1
        fi
    fi

    if ! git -C "$BASH_SOURCE_DIR" cat-file -e "$whp_bash_expected^{commit}" 2>/dev/null; then
        if [ "$WHP_SUBMODULES" != 1 ]; then
            printf 'WHP Bash: pinned commit %s is unavailable and WHP_SUBMODULES=0\n' \
                "$whp_bash_expected" >&2
            unset whp_bash_expected
            return 1
        fi
        printf 'WHP Bash: fetching pinned source object %s\n' "$whp_bash_expected" >&2
        if ! git -C "$BASH_SOURCE_DIR" fetch --no-tags --depth 1 origin "$whp_bash_expected"; then
            printf 'WHP Bash: failed to fetch pinned source object %s\n' "$whp_bash_expected" >&2
            unset whp_bash_expected
            return 1
        fi
    fi

    whp_bash_stage_state="$BASH_STAGE_DIR/.whp-source"
    if [ -f "$whp_bash_stage_state" ] &&
       [ "$(cat "$whp_bash_stage_state")" = "$whp_bash_expected" ] &&
       [ -f "$BASH_STAGE_DIR/configure" ]; then
        BASH_EFFECTIVE_SOURCE_DIR=$BASH_STAGE_DIR
        export BASH_EFFECTIVE_SOURCE_DIR
        printf 'WHP Bash source: cached pinned snapshot %s\n' "$whp_bash_expected" >&2
        unset whp_bash_expected whp_bash_stage_state
        return 0
    fi

    whp_bash_stage_tmp="$BASH_STAGE_DIR.tmp.$$"
    rm -rf "$whp_bash_stage_tmp"
    mkdir -p "$whp_bash_stage_tmp"
    if ! git -C "$BASH_SOURCE_DIR" archive "$whp_bash_expected" |
         tar -xf - -C "$whp_bash_stage_tmp"; then
        rm -rf "$whp_bash_stage_tmp"
        printf 'WHP Bash: failed to stage pinned source %s\n' "$whp_bash_expected" >&2
        unset whp_bash_expected whp_bash_stage_state whp_bash_stage_tmp
        return 1
    fi
    printf '%s\n' "$whp_bash_expected" > "$whp_bash_stage_tmp/.whp-source"
    rm -rf "$BASH_STAGE_DIR"
    mv "$whp_bash_stage_tmp" "$BASH_STAGE_DIR"

    BASH_EFFECTIVE_SOURCE_DIR=$BASH_STAGE_DIR
    export BASH_EFFECTIVE_SOURCE_DIR
    printf 'WHP Bash source: staged pinned snapshot %s\n' "$whp_bash_expected" >&2
    unset whp_bash_expected whp_bash_stage_state whp_bash_stage_tmp
}

bash_source_id()
{
    whp_bash_source_id=
    if [ -f "$BASH_EFFECTIVE_SOURCE_DIR/.whp-source" ]; then
        whp_bash_source_id=$(cat "$BASH_EFFECTIVE_SOURCE_DIR/.whp-source")
    elif [ -d "$BASH_EFFECTIVE_SOURCE_DIR" ]; then
        whp_bash_source_id=$(git -C "$BASH_EFFECTIVE_SOURCE_DIR" rev-parse HEAD 2>/dev/null || true)
    fi
    if [ -z "$whp_bash_source_id" ] && [ -f "$BASH_EFFECTIVE_SOURCE_DIR/configure" ]; then
        whp_bash_source_id=$(cksum "$BASH_EFFECTIVE_SOURCE_DIR/configure"             "$BASH_EFFECTIVE_SOURCE_DIR/patchlevel.h" 2>/dev/null |
            awk '{ printf "%s:%s;", $1, $2 }')
    fi
    printf '%s\n' "$whp_bash_source_id"
    unset whp_bash_source_id
}

validate_bash_executor()
{
    whp_bash_cmd=$1
    [ -x "$whp_bash_cmd" ] || die "selected Bash executable is not usable: $whp_bash_cmd"
    "$whp_bash_cmd" --noprofile --norc -c ': & wait -n' >/dev/null 2>&1 ||
        die "selected Bash executable does not support wait -n: $whp_bash_cmd"
    unset whp_bash_cmd
}

bootstrap_bash()
{
    [ -f "$BASH_EFFECTIVE_SOURCE_DIR/configure" ] ||
        die "WHP Bash source tree is missing: $BASH_EFFECTIVE_SOURCE_DIR"

    whp_bash_make=${MAKE:-}
    if [ -z "$whp_bash_make" ]; then
        whp_bash_make=$(command -v gmake 2>/dev/null || command -v make 2>/dev/null || true)
    fi
    [ -n "$whp_bash_make" ] || die "make is required to bootstrap WHP Bash"

    whp_bash_cc=${CC_FOR_BUILD:-${CC:-}}
    [ -n "$whp_bash_cc" ] || whp_bash_cc=$(command -v clang 2>/dev/null || command -v cc 2>/dev/null || true)
    [ -n "$whp_bash_cc" ] || die "a host C compiler is required to bootstrap WHP Bash"
    whp_bash_cc_version=$("$whp_bash_cc" --version 2>/dev/null | sed -n '1p')
    whp_bash_source=$(bash_source_id)
    [ -n "$whp_bash_source" ] || die "could not identify the WHP Bash source revision"
    whp_bash_signature=$(printf '%s\n' \
        "BASH_BOOTSTRAP_RECIPE=$BASH_BOOTSTRAP_RECIPE" \
        "BASH_SOURCE=$whp_bash_source" \
        "CC=$whp_bash_cc" \
        "CC_VERSION=$whp_bash_cc_version" \
        "SDKROOT=${SDKROOT:-}" \
        "CFLAGS=${CFLAGS_FOR_BUILD:--O2}" \
        "FEATURES=minimal,job-control,no-nls,system-malloc")

    if [ -x "$BASH_BOOTSTRAP_DIR/bash" ] &&
       [ -f "$BASH_BOOTSTRAP_STATE_FILE" ] &&
       [ "$(cat "$BASH_BOOTSTRAP_STATE_FILE")" = "$whp_bash_signature" ]; then
        validate_bash_executor "$BASH_BOOTSTRAP_DIR/bash"
        WHP_BASH_CMD="$BASH_BOOTSTRAP_DIR/bash"
        CONFIG_SHELL=$WHP_BASH_CMD
        export WHP_BASH_CMD CONFIG_SHELL
        printf 'WHP Bash: cached %s\n' "$whp_bash_source" >&2
        unset whp_bash_make whp_bash_cc whp_bash_cc_version whp_bash_source whp_bash_signature
        return
    fi

    rm -rf "$BASH_BOOTSTRAP_DIR"
    mkdir -p "$BASH_BOOTSTRAP_DIR"
    whp_bash_jobs=$(detect_jobs)
    (
        cd "$BASH_BOOTSTRAP_DIR"
        CONFIG_SHELL=/bin/sh CC="$whp_bash_cc" CFLAGS="${CFLAGS_FOR_BUILD:--O2}" \
            "$BASH_EFFECTIVE_SOURCE_DIR/configure" \
                --enable-minimal-config \
                --enable-job-control \
                --disable-nls \
                --without-bash-malloc
        "$whp_bash_make" -j"$whp_bash_jobs" bash
    )

    validate_bash_executor "$BASH_BOOTSTRAP_DIR/bash"
    printf '%s\n' "$whp_bash_signature" > "$BASH_BOOTSTRAP_STATE_FILE"
    WHP_BASH_CMD="$BASH_BOOTSTRAP_DIR/bash"
    CONFIG_SHELL=$WHP_BASH_CMD
    export WHP_BASH_CMD CONFIG_SHELL
    printf 'WHP Bash: built %s\n' "$whp_bash_source" >&2
    unset whp_bash_make whp_bash_cc whp_bash_cc_version whp_bash_source whp_bash_signature whp_bash_jobs
}

prepare_bash_toolchain()
{
    if [ -n "${WHP_BASH_CMD:-}" ]; then
        validate_bash_executor "$WHP_BASH_CMD"
        CONFIG_SHELL=$WHP_BASH_CMD
        export WHP_BASH_CMD CONFIG_SHELL
        printf 'WHP Bash: explicit executor %s\n' "$WHP_BASH_CMD" >&2
        return
    fi

    case "$WATER_BASH_BOOTSTRAP" in
        n|0)
            return
            ;;
        y|1)
            stage_pinned_bash_source
            bootstrap_bash
            return
            ;;
        auto)
            [ "$WATER_LIBCXX" = llvm ] || return
            whp_bash_archs=$(selected_llvm_libcxx_archs)
            set -- $whp_bash_archs
            whp_bash_jobs=$(detect_jobs)
            if [ "$#" -lt 2 ] || [ "$whp_bash_jobs" -lt 2 ]; then
                printf 'WHP Bash: skipped; bootstrap has no useful parallel work\n' >&2
                unset whp_bash_archs whp_bash_jobs
                return
            fi
            unset whp_bash_archs whp_bash_jobs

            if ! stage_pinned_bash_source; then
                printf 'WHP Bash: pinned source staging failed; continuing serially\n' >&2
                return
            fi
            if ! ( bootstrap_bash ); then
                printf 'WHP Bash: optional bootstrap failed; continuing serially\n' >&2
                return
            fi

            WHP_BASH_CMD="$BASH_BOOTSTRAP_DIR/bash"
            validate_bash_executor "$WHP_BASH_CMD"
            CONFIG_SHELL=$WHP_BASH_CMD
            export WHP_BASH_CMD CONFIG_SHELL
            ;;
    esac
}

libcxx_ms_target()
{
    case "$1" in
        i386)    printf '%s\n' i686-pc-windows-msvc ;;
        x86_64)  printf '%s\n' x86_64-pc-windows-msvc ;;
        aarch64) printf '%s\n' aarch64-pc-windows-msvc ;;
        arm64ec) printf '%s\n' arm64ec-pc-windows-msvc ;;
        *)       return 1 ;;
    esac
}

prepare_llvm_msvcrt_headers()
{
    whp_msvcrt_source="$SOURCE_DIR/include/msvcrt"
    whp_msvcrt_overlay="$LLVM_LIBCXX_RUNTIME_DIR/msvcrt-headers"
    whp_msvcrt_state_file="$whp_msvcrt_overlay/.whp-state"

    [ -d "$whp_msvcrt_source" ] ||
        die "Water MSVCRT header tree is missing: $whp_msvcrt_source"

    whp_msvcrt_state=$(
        find "$whp_msvcrt_source" -type f -name '*.h' -print |
        LC_ALL=C sort |
        while IFS= read -r whp_msvcrt_header; do
            cksum "$whp_msvcrt_header"
        done |
        cksum |
        awk '{ printf "%s:%s", $1, $2 }'
    )

    if [ -f "$whp_msvcrt_state_file" ] &&
       [ "$(cat "$whp_msvcrt_state_file")" = "$whp_msvcrt_state" ]; then
        printf '%s\n' "$whp_msvcrt_overlay"
        unset whp_msvcrt_source whp_msvcrt_overlay whp_msvcrt_state_file whp_msvcrt_state whp_msvcrt_header
        return
    fi

    mkdir -p "$LLVM_LIBCXX_RUNTIME_DIR"
    whp_msvcrt_tmp=$(mktemp -d "${whp_msvcrt_overlay}.tmp.XXXXXX") ||
        die "failed to create filtered MSVCRT staging directory"

    find "$whp_msvcrt_source" -type f -name '*.h' -print |
    LC_ALL=C sort |
    while IFS= read -r whp_msvcrt_header; do
        whp_msvcrt_rel=${whp_msvcrt_header#"$whp_msvcrt_source"/}
        whp_msvcrt_dir=$(dirname "$whp_msvcrt_rel")
        mkdir -p "$whp_msvcrt_tmp/$whp_msvcrt_dir"
        cp -f "$whp_msvcrt_header" "$whp_msvcrt_tmp/$whp_msvcrt_rel"
    done

    printf '%s\n' "$whp_msvcrt_state" > "$whp_msvcrt_tmp/.whp-state"
    rm -rf "$whp_msvcrt_overlay"
    mv "$whp_msvcrt_tmp" "$whp_msvcrt_overlay"

    [ ! -e "$whp_msvcrt_overlay/__config" ] ||
        die "filtered MSVCRT headers unexpectedly contain legacy libc++ __config"

    printf '%s\n' "$whp_msvcrt_overlay"
    unset whp_msvcrt_source whp_msvcrt_overlay whp_msvcrt_state_file whp_msvcrt_state \
        whp_msvcrt_tmp whp_msvcrt_header whp_msvcrt_rel whp_msvcrt_dir
}

audit_llvm_libcxx_archive()
{
    whp_libcxx_audit_archive=$1
    whp_libcxx_audit_target=$2
    whp_libcxx_audit_nm=$3

    if ! "$whp_libcxx_audit_nm" --defined-only --demangle "$whp_libcxx_audit_archive" 2>/dev/null |
         grep -F 'std::__1::mutex::lock' >/dev/null; then
        printf 'WHP libc++ %s: archive is missing std::__1::mutex::lock\n' "$whp_libcxx_audit_target" >&2
        unset whp_libcxx_audit_archive whp_libcxx_audit_target whp_libcxx_audit_nm
        return 1
    fi

    whp_libcxx_audit_strings=$(command -v strings 2>/dev/null || true)
    if [ -n "$whp_libcxx_audit_strings" ]; then
        for whp_libcxx_audit_defaultlib in \
            msvcrt.lib msvcrtd.lib msvcprt.lib msvcprtd.lib \
            libcmt.lib libcmtd.lib libcpmt.lib libcpmtd.lib oldnames.lib
        do
            if "$whp_libcxx_audit_strings" "$whp_libcxx_audit_archive" 2>/dev/null |
               grep -F "$whp_libcxx_audit_defaultlib" >/dev/null; then
                printf 'WHP libc++ %s: archive embeds forbidden MSVC default library %s\n' \
                    "$whp_libcxx_audit_target" "$whp_libcxx_audit_defaultlib" >&2
                unset whp_libcxx_audit_archive whp_libcxx_audit_target whp_libcxx_audit_nm \
                    whp_libcxx_audit_strings whp_libcxx_audit_defaultlib
                return 1
            fi
        done
    fi

    unset whp_libcxx_audit_archive whp_libcxx_audit_target whp_libcxx_audit_nm \
        whp_libcxx_audit_strings whp_libcxx_audit_defaultlib
    return 0
}

prepare_one_llvm_libcxx()
{
    whp_libcxx_arch=$1
    whp_libcxx_target=$(libcxx_ms_target "$whp_libcxx_arch") ||
        die "no LLVM libc++ Microsoft-ABI target mapping for $whp_libcxx_arch"

    eval "whp_libcxx_user_cflags=\${${whp_libcxx_arch}_CXX_PE_CFLAGS:-}"
    eval "whp_libcxx_user_libs=\${${whp_libcxx_arch}_CXX_PE_LIBS:-}"
    if [ -n "$whp_libcxx_user_cflags" ] || [ -n "$whp_libcxx_user_libs" ]; then
        [ -n "$whp_libcxx_user_cflags" ] && [ -n "$whp_libcxx_user_libs" ] ||
            die "$whp_libcxx_arch C++ provider override must set both ${whp_libcxx_arch}_CXX_PE_CFLAGS and ${whp_libcxx_arch}_CXX_PE_LIBS"
        printf 'WHP libc++ %s: explicit provider override\n' "$whp_libcxx_arch" >&2
        WHP_LIBCXX_STATE="${WHP_LIBCXX_STATE:+$WHP_LIBCXX_STATE;}$whp_libcxx_arch:override"
        unset whp_libcxx_arch whp_libcxx_target whp_libcxx_user_cflags whp_libcxx_user_libs
        return
    fi

    [ -n "$LLVM_BIN" ] && [ -x "$LLVM_BIN/clang" ] && [ -x "$LLVM_BIN/clang++" ] ||
        die "WATER_LIBCXX=llvm requires a usable Clang toolchain"
    [ -f "$LLVM_SOURCE_DIR/runtimes/CMakeLists.txt" ] ||
        die "LLVM runtimes source tree is missing: $LLVM_SOURCE_DIR/runtimes"

    whp_libcxx_cmake=$(command -v cmake 2>/dev/null || true)
    [ -n "$whp_libcxx_cmake" ] || die "CMake is required to build LLVM libc++"

    whp_libcxx_ar=${AR:-}
    [ -n "$whp_libcxx_ar" ] || whp_libcxx_ar=$(command -v llvm-ar 2>/dev/null || command -v ar 2>/dev/null || true)
    [ -n "$whp_libcxx_ar" ] || die "an archiver is required to build LLVM libc++"
    whp_libcxx_ranlib=${RANLIB:-}
    [ -n "$whp_libcxx_ranlib" ] || whp_libcxx_ranlib=$(command -v llvm-ranlib 2>/dev/null || command -v ranlib 2>/dev/null || true)
    [ -n "$whp_libcxx_ranlib" ] || die "ranlib is required to build LLVM libc++"
    whp_libcxx_rc="$LLVM_BIN/llvm-rc"
    [ -x "$whp_libcxx_rc" ] ||
        die "WATER_LIBCXX=llvm requires llvm-rc in the selected LLVM toolchain: $whp_libcxx_rc"
    whp_libcxx_nm="$LLVM_BIN/llvm-nm"
    [ -x "$whp_libcxx_nm" ] ||
        die "WATER_LIBCXX=llvm requires llvm-nm in the selected LLVM toolchain: $whp_libcxx_nm"

    whp_libcxx_crt_headers=$(prepare_llvm_msvcrt_headers)
    [ -f "$whp_libcxx_crt_headers/corecrt.h" ] &&
    [ -f "$whp_libcxx_crt_headers/vcruntime_exception.h" ] &&
    [ -f "$whp_libcxx_crt_headers/vcruntime_typeinfo.h" ] &&
    [ -f "$whp_libcxx_crt_headers/new.h" ] ||
        die "filtered MSVCRT header set is incomplete"

    whp_libcxx_build="$LLVM_LIBCXX_RUNTIME_DIR/$whp_libcxx_arch"
    whp_libcxx_provider="$whp_libcxx_build/provider"
    whp_libcxx_state_file="$whp_libcxx_build/.whp-state"
    whp_libcxx_headers="$whp_libcxx_build/include/c++/v1"
    whp_libcxx_source=$(llvm_libcxx_source_id)
    whp_libcxx_compiler=$("$LLVM_BIN/clang++" --version 2>/dev/null | sed -n '1p')
    whp_libcxx_cmake_version=$("$whp_libcxx_cmake" --version 2>/dev/null | sed -n '1p')
    whp_libcxx_signature=$(printf '%s\n' \
        "LLVM_LIBCXX_RECIPE=$LLVM_LIBCXX_RECIPE" \
        "LLVM_SOURCE=$whp_libcxx_source" \
        "TARGET=$whp_libcxx_target" \
        "CXX=$LLVM_BIN/clang++" \
        "CXX_VERSION=$whp_libcxx_compiler" \
        "AR=$whp_libcxx_ar" \
        "RANLIB=$whp_libcxx_ranlib" \
        "RC=$whp_libcxx_rc" \
        "CRT_HEADERS=$(cat "$whp_libcxx_crt_headers/.whp-state")" \
        "CMAKE=$whp_libcxx_cmake_version" \
        "BUILD_TYPE=$WATER_LLVM_BUILD_TYPE" \
        "ABI=vcruntime" \
        "THREAD_API=win32" \
        "THREADS=enabled" \
        "STATIC_VISIBILITY=disabled" \
        "MSVC_DEFAULTLIB=omitted" \
        "RTLIB_DEFAULTLIB=omitted" \
        "AUTO_LINK=disabled" \
        "WATER_INCLUDE=$SOURCE_DIR/include")

    whp_libcxx_cached=0
    if [ -f "$whp_libcxx_state_file" ] &&
       [ -f "$whp_libcxx_provider/libwhp-libcxx.a" ] &&
       [ -f "$whp_libcxx_headers/__config_site" ] &&
       [ "$(cat "$whp_libcxx_state_file")" = "$whp_libcxx_signature" ]; then
        if audit_llvm_libcxx_archive "$whp_libcxx_provider/libwhp-libcxx.a" \
             "$whp_libcxx_target" "$whp_libcxx_nm"; then
            whp_libcxx_cached=1
        else
            printf 'WHP libc++ %s: cached runtime failed ABI/archive audit; rebuilding\n' "$whp_libcxx_arch" >&2
        fi
    fi

    if [ "$whp_libcxx_cached" = 0 ]; then
        rm -rf "$whp_libcxx_build"
        mkdir -p "$whp_libcxx_build" "$whp_libcxx_provider"

        set -- \
            -S "$LLVM_SOURCE_DIR/runtimes" \
            -B "$whp_libcxx_build" \
            "-DCMAKE_BUILD_TYPE=$WATER_LLVM_BUILD_TYPE" \
            -DCMAKE_SYSTEM_NAME=Windows \
            "-DCMAKE_C_COMPILER=$LLVM_BIN/clang" \
            "-DCMAKE_CXX_COMPILER=$LLVM_BIN/clang++" \
            "-DCMAKE_C_COMPILER_TARGET=$whp_libcxx_target" \
            "-DCMAKE_CXX_COMPILER_TARGET=$whp_libcxx_target" \
            "-DCMAKE_AR=$whp_libcxx_ar" \
            "-DCMAKE_RANLIB=$whp_libcxx_ranlib" \
            "-DCMAKE_RC_COMPILER=$whp_libcxx_rc" \
            -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
            -DCMAKE_C_COMPILER_WORKS=ON \
            -DCMAKE_CXX_COMPILER_WORKS=ON \
            "-DCMAKE_C_FLAGS=-D__WINE_PE_BUILD -fshort-wchar -fms-omit-default-lib -fno-rtlib-defaultlib --no-default-config -idirafter$whp_libcxx_crt_headers" \
            "-DCMAKE_CXX_FLAGS=-D__WINE_PE_BUILD -fshort-wchar -fms-omit-default-lib -fno-rtlib-defaultlib --no-default-config -idirafter$whp_libcxx_crt_headers" \
            "-DCMAKE_MSVC_RUNTIME_LIBRARY=" \
            "-DCMAKE_C_STANDARD_INCLUDE_DIRECTORIES=$SOURCE_DIR/include" \
            "-DCMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES=$SOURCE_DIR/include" \
            "-DLLVM_DEFAULT_TARGET_TRIPLE=$whp_libcxx_target" \
            -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
            -DLLVM_ENABLE_RUNTIMES:STRING=libcxx \
            -DLLVM_INCLUDE_TESTS=OFF \
            -DLLVM_INCLUDE_DOCS=OFF \
            -DLLIBCXX_ENABLE_SHARED=OFF \
            -DLLIBCXX_ENABLE_STATIC=ON \
            -DLLIBCXX_INSTALL_STATIC_LIBRARY=OFF \
            -DLLIBCXX_INSTALL_SHARED_LIBRARY=OFF \
            -DLLIBCXX_INCLUDE_TESTS=OFF \
            -DLLIBCXX_INCLUDE_BENCHMARKS=OFF \
            -DLLIBCXX_INCLUDE_DOCS=OFF \
            -DLLIBCXX_ENABLE_ABI_LINKER_SCRIPT=OFF \
            -DLIBCXX_CXX_ABI:STRING=vcruntime \
            -DLLIBCXX_ABI_FORCE_MICROSOFT=ON \
            -DLIBCXX_ENABLE_THREADS=ON \
            -DLIBCXX_HERMETIC_STATIC_LIBRARY=ON \
            -DLLIBCXX_HAS_WIN32_THREAD_API=ON \
            -DLLIBCXX_HAS_PTHREAD_API=OFF \
            -DLLIBCXX_ENABLE_STATIC_ABI_LIBRARY=OFF \
            -DLLIBCXX_STATICALLY_LINK_ABI_IN_STATIC_LIBRARY=OFF \
            -DLIBCXX_STATIC_OUTPUT_NAME=whp-libcxx

        whp_libcxx_ninja=$(find_existing_ninja)
        if [ -n "$whp_libcxx_ninja" ]; then
            set -- "$@" -G Ninja "-DCMAKE_MAKE_PROGRAM=$whp_libcxx_ninja"
        fi

        whp_libcxx_saved_path=$PATH
        PATH="$LLVM_BIN:$PATH"
        export PATH
        "$whp_libcxx_cmake" "$@"

        whp_libcxx_abi=$(sed -n 's/^LIBCXX_CXX_ABI:STRING=//p' "$whp_libcxx_build/CMakeCache.txt" | sed -n '1p')
        [ "$whp_libcxx_abi" = vcruntime ] ||
            die "LLVM libc++ selected unexpected C++ ABI provider '${whp_libcxx_abi:-unknown}' for $whp_libcxx_target; expected vcruntime"
        whp_libcxx_runtimes=$(sed -n 's/^LLVM_ENABLE_RUNTIMES:STRING=//p' "$whp_libcxx_build/CMakeCache.txt" | sed -n '1p')
        [ "$whp_libcxx_runtimes" = libcxx ] ||
            die "LLVM runtime set changed unexpectedly to '${whp_libcxx_runtimes:-unknown}' for $whp_libcxx_target; expected libcxx only"
        whp_libcxx_threads=$(sed -n 's/^LIBCXX_ENABLE_THREADS:BOOL=//p' "$whp_libcxx_build/CMakeCache.txt" | sed -n '1p')
        [ "$whp_libcxx_threads" = ON ] ||
            die "LLVM libc++ disabled threads for $whp_libcxx_target"
        whp_libcxx_hermetic=$(sed -n 's/^LIBCXX_HERMETIC_STATIC_LIBRARY:BOOL=//p' "$whp_libcxx_build/CMakeCache.txt" | sed -n '1p')
        [ "$whp_libcxx_hermetic" = ON ] ||
            die "LLVM libc++ disabled hermetic static-library mode for $whp_libcxx_target"
        whp_libcxx_msvc_runtime=$(sed -n 's/^CMAKE_MSVC_RUNTIME_LIBRARY:[^=]*=//p' "$whp_libcxx_build/CMakeCache.txt" | sed -n '1p')
        [ -z "$whp_libcxx_msvc_runtime" ] ||
            die "CMake injected MSVC runtime '$whp_libcxx_msvc_runtime' for $whp_libcxx_target"

        whp_libcxx_jobs=${WHP_LIBCXX_JOBS:-$(detect_jobs)}
        case "$whp_libcxx_jobs" in
            ''|*[!0-9]*|0) die "WHP_LIBCXX_JOBS must be a positive integer" ;;
        esac
        "$whp_libcxx_cmake" --build "$whp_libcxx_build" --parallel "$whp_libcxx_jobs" --target cxx_static
        PATH=$whp_libcxx_saved_path
        export PATH

        whp_libcxx_archive=$(find "$whp_libcxx_build" -type f \
            \( -name 'libwhp-libcxx.a' -o -name 'libwhp-libcxx.lib' -o -name 'whp-libcxx.lib' \) \
            -print | sed -n '1p')
        [ -n "$whp_libcxx_archive" ] ||
            die "LLVM libc++ did not produce a static archive for $whp_libcxx_arch"
        [ -f "$whp_libcxx_headers/__config_site" ] ||
            die "LLVM libc++ did not generate __config_site for $whp_libcxx_arch"
        cp "$whp_libcxx_archive" "$whp_libcxx_provider/libwhp-libcxx.a"
        audit_llvm_libcxx_archive "$whp_libcxx_provider/libwhp-libcxx.a" \
            "$whp_libcxx_target" "$whp_libcxx_nm" ||
            die "LLVM libc++ archive failed Water's Microsoft-ABI static-provider audit for $whp_libcxx_target"

        whp_libcxx_probe="$whp_libcxx_build/.whp-libcxx-probe.cpp"
        cat > "$whp_libcxx_probe" <<'EOF'
#include <__config>
#if _LIBCPP_VERSION < 240000
# error WHP libc++ provider is older than the pinned LLVM libc++
#endif
#ifndef _LIBCPP_ABI_VCRUNTIME
# error WHP libc++ provider is not using the vcruntime ABI
#endif
#include <mutex>
#include <string>
int whp_libcxx_probe(std::mutex& mutex) {
    mutex.lock();
    mutex.unlock();
    return std::string("whp").size() == 3 ? 0 : 1;
}
EOF
        "$LLVM_BIN/clang++" -target "$whp_libcxx_target" --no-default-config \
            -std=c++17 -fshort-wchar -fms-omit-default-lib -fno-rtlib-defaultlib \
            -D__WINE_PE_BUILD -D_LIBCPP_NO_AUTO_LINK \
            -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -nostdinc++ \
            "-I$whp_libcxx_headers" \
            -isystem "$SOURCE_DIR/include" -isystem "$SOURCE_DIR/include/msvcrt" \
            -c "$whp_libcxx_probe" -o "$whp_libcxx_build/.whp-libcxx-probe.o"
        rm -f "$whp_libcxx_probe" "$whp_libcxx_build/.whp-libcxx-probe.o"

        printf '%s\n' "$whp_libcxx_signature" > "$whp_libcxx_state_file"
        printf 'WHP libc++ %s: built LLVM libc++ %s\n' "$whp_libcxx_arch" "$whp_libcxx_source" >&2
    else
        printf 'WHP libc++ %s: cached LLVM runtime\n' "$whp_libcxx_arch" >&2
    fi

    # Microsoft-ABI libc++ headers default to DLL import annotations and
    # /DEFAULTLIB:c++.lib. Water consumes a private static provider instead.
    whp_libcxx_cflags="-D_LIBCPP_NO_AUTO_LINK -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS -nostdinc++ -I$whp_libcxx_headers"
    whp_libcxx_libs="-L$whp_libcxx_provider -lwhp-libcxx vcruntime140"
    export "${whp_libcxx_arch}_CXX_PE_CFLAGS=$whp_libcxx_cflags"
    export "${whp_libcxx_arch}_CXX_PE_LIBS=$whp_libcxx_libs"
    whp_libcxx_state_sum=$(cksum "$whp_libcxx_state_file" | awk '{ printf "%s:%s", $1, $2 }')
    WHP_LIBCXX_STATE="${WHP_LIBCXX_STATE:+$WHP_LIBCXX_STATE;}$whp_libcxx_arch:$whp_libcxx_state_sum"

    unset whp_libcxx_arch whp_libcxx_target whp_libcxx_user_cflags whp_libcxx_user_libs \
        whp_libcxx_cmake whp_libcxx_cmake_version whp_libcxx_ar whp_libcxx_ranlib whp_libcxx_rc whp_libcxx_nm whp_libcxx_crt_headers \
        whp_libcxx_build whp_libcxx_provider whp_libcxx_state_file whp_libcxx_headers \
        whp_libcxx_source whp_libcxx_compiler whp_libcxx_signature whp_libcxx_cached \
        whp_libcxx_ninja whp_libcxx_saved_path whp_libcxx_archive whp_libcxx_probe \
        whp_libcxx_abi whp_libcxx_runtimes whp_libcxx_threads whp_libcxx_hermetic \
        whp_libcxx_msvc_runtime whp_libcxx_jobs \
        whp_libcxx_cflags whp_libcxx_libs whp_libcxx_state_sum
}

prepare_libcxx_provider()
{
    WHP_LIBCXX_STATE=$WATER_LIBCXX
    export WHP_LIBCXX_STATE

    if [ "$WATER_LIBCXX" = legacy ]; then
        printf 'WHP libc++ provider: legacy Water libc++ (_LIBCPP_VERSION 8000)\n' >&2
        return
    fi

    whp_libcxx_selected=$(selected_libcxx_archs)
    for whp_libcxx_arch in $whp_libcxx_selected
    do
        case "$whp_libcxx_arch" in
            arm)
                printf 'WHP libc++ arm: legacy provider retained for armv7-windows-gnu ABI\n' >&2
                ;;
            powerpc)
                printf 'WHP libc++ powerpc: legacy provider retained pending PowerPC COFF runtime support\n' >&2
                ;;
        esac
    done
    whp_libcxx_prepare=$(selected_llvm_libcxx_archs)

    set -- $whp_libcxx_prepare
    whp_libcxx_count=$#
    if [ "$whp_libcxx_count" -gt 1 ] && [ -n "${WHP_BASH_CMD:-}" ]; then
        whp_libcxx_total_jobs=$(detect_jobs)
        whp_libcxx_parallel=$whp_libcxx_count
        if [ "$whp_libcxx_parallel" -gt "$whp_libcxx_total_jobs" ]; then
            whp_libcxx_parallel=$whp_libcxx_total_jobs
        fi
        whp_libcxx_jobs=$((whp_libcxx_total_jobs / whp_libcxx_parallel))
        [ "$whp_libcxx_jobs" -gt 0 ] || whp_libcxx_jobs=1

        # Materialize the shared header overlay once before parallel workers.
        prepare_llvm_msvcrt_headers >/dev/null
        printf 'WHP libc++: %s providers in parallel (%s workers, %s jobs each)\n' \
            "$whp_libcxx_count" "$whp_libcxx_parallel" "$whp_libcxx_jobs" >&2

        "$WHP_BASH_CMD" --noprofile --norc -c '
            script=$1
            limit=$2
            jobs=$3
            shift 3
            running=0
            failed=0
            for arch
            do
                WHP_GIT_UPDATE=0 WHP_SUBMODULES=0 WHP_RECONFIGURE=0 WHP_LIBCXX_JOBS=$jobs \
                    "$script" __libcxx_one "$arch" &
                running=$((running + 1))
                if [ "$running" -ge "$limit" ]; then
                    wait -n || failed=1
                    running=$((running - 1))
                fi
            done
            while [ "$running" -gt 0 ]
            do
                wait -n || failed=1
                running=$((running - 1))
            done
            exit "$failed"
        ' whp-libcxx "$SOURCE_DIR/build.sh" "$whp_libcxx_parallel" "$whp_libcxx_jobs" $whp_libcxx_prepare ||
            die "parallel LLVM libc++ provider bootstrap failed"

        # Re-enter each provider in the parent shell. Cache hits are cheap and
        # publish the architecture-specific CXX_PE_* variables to configure.
        for whp_libcxx_arch in $whp_libcxx_prepare
        do
            prepare_one_llvm_libcxx "$whp_libcxx_arch"
        done
    else
        for whp_libcxx_arch in $whp_libcxx_prepare
        do
            prepare_one_llvm_libcxx "$whp_libcxx_arch"
        done
    fi

    export WHP_LIBCXX_STATE
    unset whp_libcxx_selected whp_libcxx_prepare whp_libcxx_arch whp_libcxx_count \
        whp_libcxx_total_jobs whp_libcxx_parallel whp_libcxx_jobs
}


profile_signature()
{
    printf '%s\n' \
        "WHP_PROFILE_SCHEMA=5" \
        "WATER_ARCHS_MODE=${WATER_ARCHS_MODE:-auto}" \
        "WATER_LLVM_BOOTSTRAP=${WATER_LLVM_BOOTSTRAP:-auto}" \
        "WATER_LLVM_BUILD_TYPE=${WATER_LLVM_BUILD_TYPE:-Release}" \
        "WATER_LLVM_ASSERTIONS=${WATER_LLVM_ASSERTIONS:-n}" \
        "WATER_LLVM_LEAN=${WATER_LLVM_LEAN:-y}" \
        "WATER_LLVM_PCH=${WATER_LLVM_PCH:-n}" \
        "WATER_LLVM_LINKER=${WATER_LLVM_LINKER:-auto}" \
        "WATER_LIBCXX=${WATER_LIBCXX:-llvm}" \
        "WATER_BASH_BOOTSTRAP=${WATER_BASH_BOOTSTRAP:-auto}" \
        "WHP_LIBCXX_STATE=${WHP_LIBCXX_STATE:-}" \
        "WHP_LLVM_LINK_JOBS=$LLVM_LINK_JOBS" \
        "WATER_COMPILER_CACHE=${WATER_COMPILER_CACHE:-auto}" \
        "BOOTSTRAP_NINJA=${BOOTSTRAP_NINJA:-auto}" \
        "NINJA_CMD=${NINJA_CMD:-}" \
        "SDKROOT=${SDKROOT:-}" \
        "MACOSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET:-}" \
        "WHP_DARWIN_SDKROOT=${WHP_DARWIN_SDKROOT:-}" \
        "WHP_HOST_CC_REAL=${WHP_HOST_CC_REAL:-}" \
        "WHP_HOST_CXX_REAL=${WHP_HOST_CXX_REAL:-}" \
        "WHP_LLVM_TOOLCHAIN_STATE=${WHP_LLVM_TOOLCHAIN_STATE:-}" \
        "WATER_SYSTEM_DLLPATH=${WATER_SYSTEM_DLLPATH:-auto}" \
        "WATER_WINE_TOOLS=${WATER_WINE_TOOLS:-auto}" \
        "WATER_WINE64=${WATER_WINE64:-auto}" \
        "CC=${CC:-}" "CXX=${CXX:-}" "AR=${AR:-}" "NM=${NM:-}" "RANLIB=${RANLIB:-}" \
        "LD=${LD:-}" "LDFLAGS=${LDFLAGS:-}" "WHP_HOST_LINKER=${WHP_HOST_LINKER:-}"

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
        WATER_WITH_KRB5:krb5 WATER_WITH_NETAPI:netapi \
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

    case "${WATER_WITH_MINGW:-auto}" in
        auto|'')
            mingw_clang=
            if [ -n "${LLVM_BIN:-}" ] && [ -x "$LLVM_BIN/clang" ]; then
                mingw_clang="$LLVM_BIN/clang"
            elif [ -n "${WHP_HOST_CC_REAL:-}" ]; then
                case "$WHP_HOST_CC_REAL" in
                    clang|*/clang) mingw_clang=$WHP_HOST_CC_REAL ;;
                esac
            else
                case "${CC:-}" in
                    clang|*/clang) mingw_clang=$CC ;;
                esac
            fi
            [ -n "$mingw_clang" ] ||
                die "WATER_WITH_MINGW=auto requires a usable LLVM clang"
            set -- "--with-mingw=$mingw_clang" "$@"
            printf 'WHP PE compiler: %s\n' "$mingw_clang" >&2
            ;;
        clang)
            if [ -n "${LLVM_BIN:-}" ] && [ -x "$LLVM_BIN/clang" ]; then
                mingw_clang="$LLVM_BIN/clang"
            else
                mingw_clang=$(command -v clang 2>/dev/null || true)
            fi
            [ -n "$mingw_clang" ] ||
                die "WATER_WITH_MINGW=clang requested but clang was not found"
            set -- "--with-mingw=$mingw_clang" "$@"
            printf 'WHP PE compiler: %s\n' "$mingw_clang" >&2
            ;;
        llvm-mingw)
            set -- "--with-mingw=llvm-mingw" "$@"
            ;;
        y|1)
            set -- "--with-mingw" "$@"
            ;;
        n|0)
            set -- "--without-mingw" "$@"
            ;;
        */clang)
            [ -x "$WATER_WITH_MINGW" ] ||
                die "WATER_WITH_MINGW clang path is not executable: $WATER_WITH_MINGW"
            set -- "--with-mingw=$WATER_WITH_MINGW" "$@"
            printf 'WHP PE compiler: %s\n' "$WATER_WITH_MINGW" >&2
            ;;
    esac

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
        ninja_cmd=$(find_existing_ninja)
        [ -n "$ninja_cmd" ] || die "build.ninja exists but Ninja was not found"
        if [ "$WATER_KEEP_GOING" = y ] || [ "$WATER_KEEP_GOING" = 1 ]; then
            "$ninja_cmd" -C "$BUILD_DIR" -j "$jobs" -k 0 "$@"
        else
            "$ninja_cmd" -C "$BUILD_DIR" -j "$jobs" "$@"
        fi
        return
    fi

    make_cmd=${MAKE:-}
    if [ -z "$make_cmd" ]; then
        make_cmd=$(command -v gmake 2>/dev/null || command -v make 2>/dev/null || true)
    fi
    [ -n "$make_cmd" ] || die "make was not found"
    if [ "$WATER_KEEP_GOING" = y ] || [ "$WATER_KEEP_GOING" = 1 ]; then
        "$make_cmd" -C "$BUILD_DIR" -j"$jobs" -k "$@"
    else
        "$make_cmd" -C "$BUILD_DIR" -j"$jobs" "$@"
    fi
}

case "${1:-build}" in
    __libcxx_one)
        [ "$#" -eq 2 ] || die "__libcxx_one requires exactly one architecture"
        load_whp_config
        validate_profile
        setup_toolchain
        prepare_one_llvm_libcxx "$2"
        exit 0
        ;;
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

case "${1:-build}" in
    clean|distclean)
        maintenance_target=$1
        shift
        if [ -f "$BUILD_DIR/Makefile" ] || [ -f "$BUILD_DIR/build.ninja" ]; then
            printf 'WHP maintenance target: %s\n' "$maintenance_target" >&2
            run_build "$maintenance_target" "$@"
        else
            printf 'WHP maintenance target %s: build tree is already absent\n' "$maintenance_target" >&2
        fi
        exit 0
        ;;
esac

update_repository
generate_configure
init_submodules
prepare_ninja
prepare_llvm_toolchain
setup_toolchain
prepare_bash_toolchain
prepare_libcxx_provider

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
