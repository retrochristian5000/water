#!/bin/sh
# Host smoke test for the Water Z_SOLO subset of the pinned ZLIB fork.
# This does not replace target-specific PE builds and runtime tests.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
source_dir=$root/libs/zlib/source
cc=${TEST_CC:-cc}
ar=${TEST_AR:-ar}

for file in zlib.h zconf.h adler32.c crc32.c deflate.c inffast.c inflate.c inftrees.c trees.c zutil.c; do
    [ -f "$source_dir/$file" ] || {
        echo "missing pinned ZLIB source: $file (run git submodule update --init libs/zlib/source)" >&2
        exit 1
    }
done

# An advanced or stale source checkout must not silently replace Water's pin.
if command -v git >/dev/null 2>&1 && git -C "$root" rev-parse --show-toplevel >/dev/null 2>&1; then
    pinned=$(git -C "$root" ls-files -s -- libs/zlib/source | awk '$1 == "160000" {print $2}')
    if [ -n "$pinned" ]; then
        current=$(git -C "$source_dir" rev-parse HEAD 2>/dev/null || true)
        [ "$current" = "$pinned" ] || {
            echo "ZLIB checkout does not match Water's pinned gitlink ($pinned)" >&2
            exit 1
        }
    fi
fi

for tool in "$cc" "$ar"; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "SKIP: host compiler/archive tool unavailable: $tool"
        exit 0
    }
done

tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-zlib-fork.XXXXXX")
trap 'rm -rf "$tmp"' 0 HUP INT TERM

for name in adler32 crc32 deflate inffast inflate inftrees trees zutil; do
    "$cc" -DZ_SOLO -DFAR= -I"$source_dir" -c "$source_dir/$name.c" -o "$tmp/$name.o"
done
"$ar" rcs "$tmp/libz.a" "$tmp/"*.o

cat > "$tmp/roundtrip.c" <<'C'
#include "zlib.h"
#include <string.h>

int main(void)
{
    static const Bytef data[] = "Water pinned ZLIB deflate/inflate roundtrip";
    Bytef compressed[256], restored[256];
    z_stream enc = {0}, dec = {0};
    uLong packed;
    int status;

    enc.next_in = (Bytef *)data;
    enc.avail_in = sizeof(data);
    enc.next_out = compressed;
    enc.avail_out = sizeof(compressed);
    if (deflateInit(&enc, Z_DEFAULT_COMPRESSION) != Z_OK) return 1;
    status = deflate(&enc, Z_FINISH);
    packed = enc.total_out;
    deflateEnd(&enc);
    if (status != Z_STREAM_END) return 2;

    dec.next_in = compressed;
    dec.avail_in = packed;
    dec.next_out = restored;
    dec.avail_out = sizeof(restored);
    if (inflateInit(&dec) != Z_OK) return 3;
    status = inflate(&dec, Z_FINISH);
    if (status != Z_STREAM_END || dec.total_out != sizeof(data) ||
        memcmp(data, restored, sizeof(data))) {
        inflateEnd(&dec);
        return 4;
    }
    inflateEnd(&dec);
    return 0;
}
C

"$cc" -DZ_SOLO -DFAR= -I"$source_dir" "$tmp/roundtrip.c" "$tmp/libz.a" -o "$tmp/roundtrip"
"$tmp/roundtrip"
echo "PASS: Water Z_SOLO sources link and round-trip through pinned ZLIB fork"
