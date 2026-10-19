#!/usr/bin/env python3
"""Compile the actual Water IO.SYS MBR and Win95 RTM VBR decoders using synthetic sectors."""
import os
from pathlib import Path
import re
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]


def function(src, name, result):
    pattern = r"(?m)^" + re.escape(result) + r"\s+" + re.escape(name) + r"\s*\([^;]*\)\s*\{"
    match = re.search(pattern, src)
    if not match:
        raise AssertionError("Function missing: " + name)
    depth = 0
    for i in range(match.end() - 1, len(src)):
        if src[i] == "{":
            depth += 1
        elif src[i] == "}":
            depth -= 1
            if depth == 0:
                return src[match.start():i + 1]
    raise AssertionError("Unterminated function: " + name)


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
__PART_STRUCT__
__C_FUNCTIONS__

static void write16(BYTE *p, WORD v) { p[0] = v; p[1] = v >> 8; }
static void write32(BYTE *p, DWORD v) { write16(p, (WORD)v); write16(p + 2, (WORD)(v >> 16)); }

static void mbr_base(BYTE sector[512], BYTE type, DWORD lba, DWORD count)
{
    BYTE *entry;
    memset(sector, 0, 512);
    sector[510] = 0x55;
    sector[511] = 0xaa;
    entry = sector + 0x1be + 16;
    entry[0] = 0x80;   /* second partition active; slot 1 */
    entry[4] = type;
    write32(entry + 8, lba);
    write32(entry + 12, count);
}

static void vbr_base(BYTE sector[512], DWORD hidden, DWORD total, int fat12)
{
    memset(sector, 0, 512);
    sector[0] = 0xeb;
    sector[1] = 0x3c;
    sector[2] = 0x90;
    write16(sector + 0x0b, 512);
    sector[0x0d] = fat12 ? 1 : 2;
    write16(sector + 0x0e, 1);
    sector[0x10] = 2;
    write16(sector + 0x11, fat12 ? 224 : 512);
    write16(sector + 0x13, total <= 65535 ? (WORD)total : 0);
    sector[0x15] = 0xf8;
    write16(sector + 0x16, fat12 ? 9 : 128);
    write32(sector + 0x1c, hidden);
    if (total > 65535) write32(sector + 0x20, total);
    sector[510] = 0x55;
    sector[511] = 0xaa;
}

static void reject_mbr(const BYTE sector[512], ULONGLONG disk)
{
    struct iosys_mbr_boot_partition out;
    struct iosys_mbr_boot_partition old;
    memset(&out, 0xa5, sizeof(out));
    old = out;
    assert(!IOSYS_ParseMbrBootPartition(sector, disk, &out));
    assert(!memcmp(&out, &old, sizeof(out)));
}

int main(void)
{
    BYTE mbr[512], vbr[512];
    struct iosys_mbr_boot_partition p, old;

    mbr_base(mbr, 0x06, 2048, 65536);
    assert(IOSYS_ParseMbrBootPartition(mbr, 100000, &p));
    assert(p.slot == 1 && p.partition_type == 0x06);
    assert(p.start_lba == 2048 && p.sector_count == 65536);
    vbr_base(vbr, p.start_lba, p.sector_count, 0);
    assert(IOSYS_ValidateWin95RtmVbr(vbr, &p));

    old = p;
    mbr[510] = 0;
    reject_mbr(mbr, 100000);
    mbr[510] = 0x55;
    mbr[0x1be + 16] = 0x01;
    reject_mbr(mbr, 100000);  /* bad active flag */
    mbr[0x1be + 16] = 0x80;
    mbr[0x1be] = 0x80;
    reject_mbr(mbr, 100000);  /* two active flags */
    mbr[0x1be] = 0;
    mbr[0x1be + 16] = 0;
    reject_mbr(mbr, 100000);  /* no active partition */
    mbr[0x1be + 16] = 0x80;
    write32(mbr + 0x1be + 16 + 8, 0);
    reject_mbr(mbr, 100000);
    write32(mbr + 0x1be + 16 + 8, 2048);
    write32(mbr + 0x1be + 16 + 12, 0);
    reject_mbr(mbr, 100000);
    write32(mbr + 0x1be + 16 + 12, 65536);
    reject_mbr(mbr, 65536);  /* beyond disk end */
    reject_mbr(mbr, 0);
    assert(!IOSYS_ParseMbrBootPartition(NULL, 100000, &p));
    assert(!IOSYS_ParseMbrBootPartition(mbr, 100000, NULL));

    assert(IOSYS_IsWin95RtmPartitionType(0x01));
    assert(IOSYS_IsWin95RtmPartitionType(0x04));
    assert(IOSYS_IsWin95RtmPartitionType(0x06));
    assert(IOSYS_IsWin95RtmPartitionType(0x0e));
    assert(!IOSYS_IsWin95RtmPartitionType(0x0b));
    assert(!IOSYS_IsWin95RtmPartitionType(0x0c));
    assert(!IOSYS_IsWin95RtmPartitionType(0x0f));
    assert(!IOSYS_IsWin95RtmPartitionType(0xee));

    p = old;
    vbr_base(vbr, p.start_lba, p.sector_count, 0);
    assert(IOSYS_ValidateWin95RtmVbr(vbr, &p));
    vbr[511] = 0;
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));
    vbr[511] = 0xaa;
    write32(vbr + 0x1c, p.start_lba + 1);
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));
    write32(vbr + 0x1c, p.start_lba);
    write32(vbr + 0x20, p.sector_count + 1); /* > partition */
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));
    write32(vbr + 0x20, p.sector_count);
    write16(vbr + 0x16, 1); /* insufficient FAT entries */
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));
    write16(vbr + 0x16, 128);
    p.partition_type = 0x0b; /* FAT32 only in OSR2 */
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));
    p.partition_type = 0x06;
    vbr[0] = 0x90; /* missing boot jump */
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));
    vbr_base(vbr, p.start_lba, p.sector_count, 0);
    assert(!IOSYS_ValidateWin95RtmVbr(NULL, &p));
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, NULL));

    /* FAT12 used on small RTM boot partitions. */
    p.partition_type = 0x01;
    p.sector_count = 2880;
    vbr_base(vbr, p.start_lba, p.sector_count, 1);
    assert(IOSYS_ValidateWin95RtmVbr(vbr, &p));
    p.partition_type = 0x06;
    assert(!IOSYS_ValidateWin95RtmVbr(vbr, &p));

    puts("iosys MBR+VBR: passed");
    return 0;
}
"""


class MbrTests(unittest.TestCase):
    def test_source_integration(self):
        src = (ROOT / "programs/io.sys/config.c").read_text()
        hdr = (ROOT / "programs/io.sys/io_sys.h").read_text()
        self.assertIn("IOSYS_ParseMbrBootPartition", hdr)
        self.assertIn("IOSYS_ValidateWin95RtmVbr", hdr)
        self.assertIn("mbr + 0x1be + i * 16", src)
        self.assertIn("mbr[510] != 0x55 || mbr[511] != 0xaa", src)
        self.assertIn("if (found) return FALSE;", src)
        self.assertIn("(ULONGLONG)start + count > disk_sectors", src)
        self.assertIn("iosys_get_dword( vbr + 0x1c ) != part->start_lba", src)
        self.assertIn("!IOSYS_IsWin95RtmPartitionType(part->partition_type)", src)
        self.assertNotIn("CreateFile", src[src.index("BOOL IOSYS_ParseMbrBootPartition"):
                                        src.index("BOOL IOSYS_ValidateWin95RtmVbr")])
        self.assertNotIn("WriteFile", src[src.index("BOOL IOSYS_ParseMbrBootPartition"):
                                       src.index("BOOL IOSYS_ValidateWin95RtmVbr")])

    def test_synthetic_sector_decoder(self):
        cmd = shlex.split(os.environ.get("CC", "cc"))
        if not cmd or not shutil.which(cmd[0]):
            self.skipTest("C compiler unavailable")
        src = (ROOT / "programs/io.sys/config.c").read_text()
        hdr = (ROOT / "programs/io.sys/io_sys.h").read_text()
        struct = re.search(r"struct iosys_mbr_boot_partition\s*\{[^}]+\};", hdr)
        self.assertIsNotNone(struct)
        names = (("iosys_get_word", "static WORD"),
                 ("iosys_get_dword", "static DWORD"),
                 ("IOSYS_ParseMbrBootPartition", "BOOL"),
                 ("IOSYS_IsWin95RtmPartitionType", "BOOL"),
                 ("IOSYS_ValidateWin95RtmVbr", "BOOL"))
        code = HARNESS.replace("__PART_STRUCT__", struct.group(0))
        code = code.replace("__C_FUNCTIONS__", "\n\n".join(function(src, n, ty) for n, ty in names))
        with tempfile.TemporaryDirectory(prefix="water-mbr-") as directory:
            testfile = Path(directory) / "test.c"
            binary = Path(directory) / "test"
            testfile.write_text(code)
            result = subprocess.run(cmd + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                                           str(testfile), "-o", str(binary)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("iosys MBR+VBR: passed", result.stdout)


if __name__ == "__main__":
    unittest.main()
