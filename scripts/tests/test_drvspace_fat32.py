#!/usr/bin/env python3
"""Compile and verify DriveSpace's read-only FAT32 exclusion policy."""
from pathlib import Path
import os
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]

WIND_DEF = r"""
#ifndef TEST_WINDEF_H
#define TEST_WINDEF_H
#include <stdint.h>
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint64_t ULONGLONG;
typedef int BOOL;
typedef int INT;
#define TRUE 1
#define FALSE 0
#endif
"""

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include "io_sys.h"
#include "volume_policy.h"

static int fat32_calls;
static int fat1216_calls;
static BOOL fat32_present;
static BOOL fat1216_present;

BOOL IOSYS_GetFat32BPB(BYTE drive, struct iosys_fat32_bpb *bpb)
{
    assert(drive < 26 && bpb);
    fat32_calls++;
    return fat32_present;
}

BOOL IOSYS_GetFat1216BPB(BYTE drive, struct iosys_fat_bpb *bpb)
{
    assert(drive < 26 && bpb);
    fat1216_calls++;
    return fat1216_present;
}

static void reset(BOOL fat32, BOOL fat1216)
{
    fat32_calls = fat1216_calls = 0;
    fat32_present = fat32;
    fat1216_present = fat1216;
}

int main(void)
{
    reset(FALSE, FALSE);
    assert(DRIVESPACE_ClassifyTarget(26) == DRIVESPACE_TARGET_UNKNOWN);
    assert(DRIVESPACE_ClassifyTarget(255) == DRIVESPACE_TARGET_UNKNOWN);
    assert(fat32_calls == 0 && fat1216_calls == 0);

    reset(FALSE, FALSE);
    assert(DRIVESPACE_ClassifyTarget(2) == DRIVESPACE_TARGET_UNKNOWN);
    assert(fat32_calls == 1 && fat1216_calls == 1);

    reset(FALSE, TRUE);
    assert(DRIVESPACE_ClassifyTarget(0) == DRIVESPACE_TARGET_FAT12_16_CANDIDATE);
    assert(fat32_calls == 1 && fat1216_calls == 1);

    reset(TRUE, FALSE);
    assert(DRIVESPACE_ClassifyTarget(2) == DRIVESPACE_TARGET_FAT32_REJECTED);
    assert(fat32_calls == 1 && fat1216_calls == 0);

    /* Detection wins even if a faulty legacy provider also claims FAT16. */
    reset(TRUE, TRUE);
    assert(DRIVESPACE_ClassifyTarget(2) == DRIVESPACE_TARGET_FAT32_REJECTED);
    assert(fat32_calls == 1 && fat1216_calls == 0);

    puts("DriveSpace FAT32 exclusion policy: passed");
    return 0;
}
"""


class DriveSpacePolicyTests(unittest.TestCase):
    def test_source_owner_and_no_writes(self):
        src = (ROOT / "programs/drvspace/volume_policy.c").read_text()
        header = (ROOT / "programs/drvspace/volume_policy.h").read_text()
        self.assertIn("DRIVESPACE_TARGET_FAT32_REJECTED", header)
        self.assertIn("DRIVESPACE_TARGET_FAT12_16_CANDIDATE", header)
        self.assertLess(src.index("IOSYS_GetFat32BPB("),
                        src.index("IOSYS_GetFat1216BPB("))
        self.assertNotIn("WriteFile(", src)
        self.assertNotIn("CreateFile", src)

    def test_policy_compiles_and_rejects_fat32(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("C compiler unavailable")
        with tempfile.TemporaryDirectory(prefix="water-drivespace-") as temp:
            directory = Path(temp)
            (directory / "windef.h").write_text(WIND_DEF)
            harness = directory / "test.c"
            harness.write_text(HARNESS)
            exe = directory / "drivespace_policy_test"
            command = compiler + [
                "-std=c99", "-Wall", "-Wextra", "-Werror",
                "-I" + str(directory),
                "-I" + str(ROOT / "programs/drvspace"),
                "-I" + str(ROOT / "programs/io.sys"),
                str(ROOT / "programs/drvspace/volume_policy.c"),
                str(harness), "-o", str(exe),
            ]
            build = subprocess.run(command, capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)
            self.assertIn("DriveSpace FAT32 exclusion policy: passed", run.stdout)


if __name__ == "__main__":
    unittest.main()
