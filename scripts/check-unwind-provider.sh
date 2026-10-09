#!/bin/sh
# Audit a GNU/Itanium-ABI LLVM libunwind candidate before retiring Water's copy.
set -eu
if [ "$#" -lt 4 ] || [ "$#" -gt 5 ]; then
    echo 'usage: check-unwind-provider.sh ARCHIVE TARGET LLVM_AR LLVM_NM [seh|sjlj]' >&2
    exit 2
fi
archive=$1
target=$2
ar=$3
nm=$4
model=seh
[ "$#" -lt 5 ] || model=$5

# CPU family alone cannot identify the C++ exception ABI.
case "$target" in
    i386*windows-gnu*|i486*windows-gnu*|i586*windows-gnu*|i686*windows-gnu*|i686*w64-mingw32*) expected=76:1; frame=yes ;;
    x86_64*windows-gnu*|x86_64*w64-mingw32*) expected=100:134; frame=yes ;;
    armv7*windows-gnu*|armv7*w64-mingw32*) expected=196:1; frame=no ;;
    *) echo "unsupported GNU unwinder target: $target" >&2; exit 2 ;;
esac
case "$model" in
    seh|sjlj) ;;
    *) echo "unsupported exception model: $model" >&2; exit 2 ;;
esac

[ -f "$archive" ] || { echo "missing unwinder archive: $archive" >&2; exit 1; }
members=$("$ar" t "$archive") || { echo "unreadable unwind archive: $archive" >&2; exit 1; }
[ -n "$members" ] || { echo "empty unwind archive: $archive" >&2; exit 1; }
duplicate=$(printf '%s\n' "$members" | LC_ALL=C sort | uniq -d | sed -n '1p')
[ -z "$duplicate" ] || { echo "duplicate archive member: $duplicate" >&2; exit 1; }

printf '%s\n' "$members" | while IFS= read -r member; do
    [ -n "$member" ] || continue
    header=$("$ar" p "$archive" "$member" | od -An -tu1 -N8)
    set -- $header
    if [ "$#" -ge 8 ] && [ "$1:$2:$3:$4" = 0:0:255:255 ] && [ "$5:$6" = 2:0 ]; then
        machine=$7:$8
    else
        machine=$1:$2
    fi
    if [ "$machine" != "$expected" ]; then
        echo "unwind member '$member': COFF machine $machine, expected $expected" >&2
        exit 1
    fi
done

symbols=$("$nm" -P --extern-only --defined-only "$archive") || {
    echo "cannot inspect unwinder symbols: $archive" >&2
    exit 1
}
has_symbol()
{
    printf '%s\n' "$symbols" | grep -Eq "^_+$1[[:space:]]"
}
if [ "$model" = sjlj ]; then
    raise=Unwind_SjLj_RaiseException
    resume=Unwind_SjLj_Resume
else
    raise=Unwind_RaiseException
    resume=Unwind_Resume
fi
if ! has_symbol "$raise" || ! has_symbol "$resume"; then
    echo "missing $model raise/resume entry points in $archive" >&2
    exit 1
fi

# LLVM 8 supplied these by default for x86, but modern LLVM requires
# -DLIBUNWIND_ENABLE_FRAME_APIS=ON for drop-in compatibility.
if [ "$frame" = yes ]; then
    if ! has_symbol register_frame_info_bases ||
       ! has_symbol deregister_frame_info_bases; then
        echo 'missing legacy x86 GCC frame-registration API' >&2
        exit 1
    fi
fi

printf 'PASS: GNU unwind COFF/ABI archive %s (%s, %s)\n' "$archive" "$target" "$model"
