#!/usr/bin/env python3
"""Compile and exercise the actual IO.SYS FAT32 decoder using fake sectors."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    pat = r"(?m)^(?:static\s+)?(?:WORD|DWORD|BOOL)\s+" + re.escape(name) + r"\s*\([^;]*\)\s*\{"
    match = re.search(pat, source)
    if not match:
        raise AssertionError("missing function " + name)
    depth = 0
    for i in range(match.end() - 1, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            depth -= 1
            if depth == 0:
                return source[match.start():i + 1]
    raise AssertionError("unterminated " + name)


HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t ULONGLONG;
typedef int BOOL;
#define TRUE 1
#define FALSE 0
__STRUCT__
__FUNCTIONS__
static void w16(BYTE *s, unsigned o, WORD v) {
    s[o] = v; s[o+1] = v >> 8;
}
static void w32(BYTE *s, unsigned o, DWORD v) {
    w16(s,o,v); w16(s,o+2,v>>16);
}
static void sample(BYTE s[512]) {
    memset(s, 0, 512);
    s[0] = 0xeb; s[1] = 0x58; s[2] = 0x90;
    w16(s,0x0b,512); s[0x0d] = 8; w16(s,0x0e,32);
    s[0x10] = 2; s[0x15] = 0xf8;
    w32(s,0x20,800000); w32(s,0x24,2048);
    w32(s,0x2c,2); w16(s,0x30,1); w16(s,0x32,6);
    s[510] = 0x55; s[511] = 0xaa;
}
static void reject(BYTE s[512]) {
    struct iosys_fat32_bpb b;
    memset(&b, 0xa5, sizeof(b));
    assert(!IOSYS_ParseFat32BPB(s, &b));
    assert(b.bytes_per_sector == 0xa5a5);
}
int main(void) {
    BYTE s[512];
    struct iosys_fat32_bpb b;
    sample(s);
    assert(IOSYS_ParseFat32BPB(s, &b));
    assert(b.bytes_per_sector == 512 && b.sectors_per_cluster == 8);
    assert(b.fat_count == 2 && b.sectors_per_fat == 2048);
    assert(b.first_data_sector == 4128);
    assert(b.data_clusters == (800000 - 4128) / 8);
    assert(b.root_cluster == 2 && b.info_sector == 1 && b.backup_boot_sector == 6);
    assert(!IOSYS_ParseFat32BPB(NULL, &b));
    assert(!IOSYS_ParseFat32BPB(s, NULL));
    sample(s); s[510] = 0; reject(s);
    sample(s); w16(s,0x0b,1023); reject(s);
    sample(s); w16(s,0x0b,8192); reject(s);
    sample(s); s[0x0d] = 3; reject(s);
    sample(s); s[0x10] = 3; reject(s);
    sample(s); w16(s,0x11,512); reject(s);
    sample(s); w16(s,0x13,128); reject(s);
    sample(s); w16(s,0x16,256); reject(s);
    sample(s); w32(s,0x24,1); reject(s);
    sample(s); w32(s,0x24,450000); reject(s);
    sample(s); w32(s,0x20,200000); reject(s);
    sample(s); w32(s,0x2c,1000000); reject(s);
    sample(s); w16(s,0x2a,1); reject(s);
    sample(s); w16(s,0x28,0x82); reject(s);
    sample(s); w16(s,0x30,40); reject(s);
    sample(s); w16(s,0x32,40); reject(s);
    puts("fat32 parser: passed");
    return 0;
}
"""


class DecoderTests(unittest.TestCase):
    def test_decoder_with_synthetic_sectors(self):
        cmd = shlex.split(os.environ.get("CC", "cc"))
        if not cmd or not shutil.which(cmd[0]):
            self.skipTest("C compiler missing")
        src = (ROOT / "programs/io.sys/config.c").read_text()
        hdr = (ROOT / "programs/io.sys/io_sys.h").read_text()
        found = re.search(r"struct iosys_fat32_bpb\s*\{[^}]+\};", hdr)
        self.assertIsNotNone(found)
        funcs = "\n\n".join(function(src, n) for n in
                            ("iosys_get_word", "iosys_get_dword", "IOSYS_ParseFat32BPB"))
        data = HARNESS.replace("__STRUCT__", found.group(0))
        data = data.replace("__FUNCTIONS__", funcs)
        with tempfile.TemporaryDirectory(prefix="water-fat32-") as tmp:
            p = Path(tmp) / "test.c"
            exe = Path(tmp) / "test"
            p.write_text(data)
            build = subprocess.run(cmd + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                                          str(p), "-o", str(exe)],
                                   capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("fat32 parser: passed", run.stdout)


if __name__ == "__main__":
    unittest.main()
