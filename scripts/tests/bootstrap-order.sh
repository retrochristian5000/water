#!/bin/sh
# Verify WATER initializes lightweight prerequisites before larger toolchains.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd -P)
source=$root/build.sh
[ -f "$source" ] || { echo 'FAIL: missing build.sh' >&2; exit 1; }

stages=$(awk '
    /^(validate_configure_rules|validate_makefile_source_continuations|prepare_native_cc_seed|prepare_ninja|prepare_autoconf_toolchain|generate_configure|init_submodules|prepare_llvm_toolchain|setup_toolchain|prepare_bash_toolchain|prepare_libcxx_provider)$/ { print }
' "$source")
expected=$(cat <<'EXPECTED'
validate_configure_rules
validate_makefile_source_continuations
prepare_native_cc_seed
prepare_ninja
prepare_autoconf_toolchain
generate_configure
init_submodules
prepare_llvm_toolchain
setup_toolchain
prepare_bash_toolchain
prepare_libcxx_provider
EXPECTED
)
if [ "$stages" != "$expected" ]; then
    printf 'FAIL: WATER bootstrap prerequisite order differs from dependency plan\nExpected:\n%s\nActual:\n%s\n' "$expected" "$stages" >&2
    exit 1
fi

# Validation is now a true preflight, not repeated after host-tool bootstrap.
if awk '/^generate_configure\(\)$/,/^}$/' "$source" |
    grep -Eq '^[[:space:]]*(validate_configure_rules|validate_makefile_source_continuations)[[:space:]]*$'
then
    echo 'FAIL: configure validators run twice' >&2
    exit 1
fi

# The small source gitlink goes first; LLVM remains lazy/conditional.
grep -Fq 'whp_submodules="libs/zlib/source libs/fluidsynth"' "$source" || {
    echo 'FAIL: zlib was not initialized ahead of Fluidsynth' >&2
    exit 1
}
grep -Fq 'whp_submodules="$whp_submodules toolchains/llvm-project"' "$source" || {
    echo 'FAIL: conditional LLVM initialization was lost' >&2
    exit 1
}
printf 'PASS: WATER lightweight preflight, Ninja/Autotools order, source gitlinks, LLVM/Bash/libc++ dependencies\n'
