#!/bin/sh
# Guard against macOS native or wrong-Windows-ABI objects being treated as
# target-matched PE output by Water configure. No LLVM rebuild is needed.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-pe-coff-machine.XXXXXXXX") || exit 1
trap 'rm -rf "$tmp"' EXIT HUP INT TERM

# Test the production functions in configure.ac, not copied logic.
for name in wine_pe_object_machine wine_pe_machine_pair_matches
do
    awk -v name="$name" '
        $0 == name "()" { copying=1 }
        copying { print }
        copying && /^}$/ { exit }
    ' "$root/configure.ac" >> "$tmp/production"
done
test "$(grep -c '^wine_pe_.*()$' "$tmp/production")" -eq 2
sh -n "$tmp/production"
. "$tmp/production"

# COFF machine numbers are little-endian; these fixtures also run with BSD od.
printf '\101\246' > "$tmp/arm64ec.o"
printf '\144\252' > "$tmp/arm64.o"
printf '\144\206' > "$tmp/x64.o"
printf '\114\001' > "$tmp/x86.o"
printf '\304\001' > "$tmp/arm.o"
printf '\360\001' > "$tmp/ppc.o"
printf '\361\001' > "$tmp/ppcfp.o"
printf '\177ELF' > "$tmp/elf.o"
printf '\317\372\355\376' > "$tmp/macho.o"
printf '\101' > "$tmp/truncated.o"

test "$(wine_pe_object_machine "$tmp/arm64ec.o")" = 65:166
test "$(wine_pe_object_machine "$tmp/arm64.o")" = 100:170
test "$(wine_pe_object_machine "$tmp/x64.o")" = 100:134
test "$(wine_pe_object_machine "$tmp/x86.o")" = 76:1
test "$(wine_pe_object_machine "$tmp/arm.o")" = 196:1
test "$(wine_pe_object_machine "$tmp/ppc.o")" = 240:1
test "$(wine_pe_object_machine "$tmp/ppcfp.o")" = 241:1
if wine_pe_object_machine "$tmp/truncated.o" >/dev/null; then
    echo 'Truncated COFF header accepted' >&2
    exit 1
fi
if wine_pe_object_machine "$tmp/missing.o" >/dev/null; then
    echo 'Missing object accepted' >&2
    exit 1
fi

for entry in \
    'i386 76:1' 'x86_64 100:134' 'arm 196:1' \
    'aarch64 100:170' 'arm64ec 65:166' 'powerpc 240:1'
do
    set -- $entry
    wine_pe_machine_pair_matches "$1" "$2" "$2" || {
        echo "Correct PE object pair rejected for $1" >&2
        exit 1
    }
done
wine_pe_machine_pair_matches powerpc 240:1 241:1
wine_pe_machine_pair_matches powerpc 241:1 240:1

# Reject macOS Mach-O/ELF, an ARM64EC C++ object paired with native ARM64 C,
# and an x64 object masquerading as native ARM64 or ARM64EC.
for pair in \
    'arm64ec 65:166 100:170' \
    'arm64ec 65:166 100:134' \
    'aarch64 100:170 65:166' \
    'x86_64 100:134 65:166' \
    'arm64ec 207:250 65:166' \
    'arm64ec 127:69 65:166' \
    'arm64ec missing 65:166' \
    'arm64ec 65:166 missing'
do
    set -- $pair
    if wine_pe_machine_pair_matches "$1" "$2" "$3"; then
        echo "Wrong-machine PE compiler pair accepted: $pair" >&2
        exit 1
    fi
done

grep -Fq 'if wine_pe_machine_pair_matches "$wine_arch" "$wine_pe_c_machine" "$wine_pe_cxx_machine"' "$root/configure.ac"
grep -Fq 'WINE_NO_PE_ERROR([test "$wine_pe_machine_ok" != yes]' "$root/configure.ac"

# Use real cross-compilers when present to catch a toolchain regression too.
if command -v clang >/dev/null 2>&1 && command -v clang++ >/dev/null 2>&1; then
    for entry in \
        'i386 i386-pc-windows-msvc' \
        'x86_64 x86_64-pc-windows-msvc' \
        'aarch64 aarch64-pc-windows-msvc' \
        'arm64ec arm64ec-pc-windows-msvc'
    do
        set -- $entry
        arch=$1 target=$2
        if printf 'int probe_c(void) { return 1; }\n' |
                clang -target "$target" -x c -c - -o "$tmp/real-c.o" >/dev/null 2>&1 &&
           printf 'extern "C" int probe_cxx(void) { return 1; }\n' |
                clang++ -target "$target" -x c++ -c - -o "$tmp/real-cxx.o" >/dev/null 2>&1
        then
            c_machine=$(wine_pe_object_machine "$tmp/real-c.o")
            cxx_machine=$(wine_pe_object_machine "$tmp/real-cxx.o")
            wine_pe_machine_pair_matches "$arch" "$c_machine" "$cxx_machine" || {
                echo "Clang emitted incorrect $arch C/C++ machine types: $c_machine $cxx_machine" >&2
                exit 1
            }
        fi
        rm -f "$tmp/real-c.o" "$tmp/real-cxx.o"
    done
fi

echo 'PASS: PE C/C++ machine matching, macOS/ARM64EC ABI isolation'
