#!/usr/bin/env python3
"""Test actual Win16 SYSTEM.DRV logic in a small portable C harness.

Extracts the real three functions, then supplies narrow Windows API shims.
This verifies logic, not Win16 NE exports, segment thunks, or timer threading.
Run: python3 scripts/tests/test_system_drv16.py
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest

SOURCE = Path(__file__).resolve().parents[2] / "dlls/system.drv16/system.c"


def function_body(source, name):
    match = re.search(
        r"\b(?:DWORD|WORD)\s+WINAPI\s+" + re.escape(name) + r"\s*\([^;]*\)\s*\{",
        source,
    )
    if not match:
        raise AssertionError("cannot find " + name)
    start = match.start()
    depth = 0
    for pos in range(match.end() - 1, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if not depth:
                return source[start:pos + 1]
    raise AssertionError("unterminated function " + name)


HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define WINAPI
typedef uint16_t WORD;
typedef uint16_t WCHAR;
typedef uint32_t DWORD;
typedef uint32_t FARPROC16;
typedef unsigned int UINT;
typedef int INT;
#define MAKELONG(lo, hi) ((DWORD)(uint16_t)(lo) | ((DWORD)(uint16_t)(hi) << 16))
#define DRIVE_UNKNOWN 0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_FIXED 3
#define DRIVE_REMOTE 4
#define DRIVE_CDROM 5
#define SYS_TIMER_RATE 54925
#define NB_SYS_TIMERS 8
#define FIXME(...) ((void)0)
#define WARN(...) ((void)0)

typedef struct {
    FARPROC16 callback16;
    INT rate, ticks;
} SYSTEM_TIMER;
static SYSTEM_TIMER SYS_Timers[NB_SYS_TIMERS];
static int SYS_NbTimers;
static unsigned starts, stops, drive_queries;

static void SYSTEM_StartTicks(void) { ++starts; }
static void SYSTEM_StopTicks(void) { ++stops; }

static WORD GetDriveTypeW(const WCHAR *root)
{
    ++drive_queries;
    /* An unrooted C: path is an error even if a particular host accepts it. */
    assert(root[0] >= 'A' && root[0] <= 'Z');
    assert(root[1] == ':');
    assert(root[2] == '\\');
    assert(root[3] == 0);
    if (root[0] == 'C') return DRIVE_CDROM;
    if (root[0] == 'D') return DRIVE_NO_ROOT_DIR;
    return DRIVE_FIXED;
}

__SYSTEM_FUNCTIONS__

int main(void)
{
    unsigned before;
    int i;

    assert(InquireSystem16(0, 0) == SYS_TIMER_RATE);
    assert(InquireSystem16(1, 2) == MAKELONG(DRIVE_REMOTE, DRIVE_REMOTE));
    assert(InquireSystem16(1, 3) == MAKELONG(DRIVE_UNKNOWN, DRIVE_UNKNOWN));
    assert(InquireSystem16(1, 25) == MAKELONG(DRIVE_FIXED, DRIVE_FIXED));
    before = drive_queries;
    assert(InquireSystem16(1, 26) == MAKELONG(DRIVE_UNKNOWN, DRIVE_UNKNOWN));
    assert(InquireSystem16(1, 0xffff) == MAKELONG(DRIVE_UNKNOWN, DRIVE_UNKNOWN));
    assert(before == drive_queries);
    assert(InquireSystem16(2, 0) == 0);
    assert(InquireSystem16(500, 0) == 0);

    /* Null callbacks may neither consume a slot nor start the timer thread. */
    assert(CreateSystemTimer16(0, 0) == 0);
    assert(SYS_NbTimers == 0 && starts == 0);
    for (i = 1; i <= 8; ++i)
    {
        assert(CreateSystemTimer16(i == 1 ? 0 : 100, i) == i);
        assert(SYS_Timers[i - 1].callback16 == (FARPROC16)i);
        assert(SYS_Timers[i - 1].rate == (i == 1 ? SYS_TIMER_RATE : 100000));
    }
    assert(SYS_NbTimers == 8 && starts == 1);
    assert(CreateSystemTimer16(10, 9) == 0);
    assert(SYSTEM_KillSystemTimer(0) == 0);
    assert(SYSTEM_KillSystemTimer(9) == 9);
    assert(SYSTEM_KillSystemTimer(1) == 0);
    assert(SYSTEM_KillSystemTimer(1) == 1);
    assert(CreateSystemTimer16(100, 99) == 1);
    for (i = 1; i <= 8; ++i)
        assert(SYSTEM_KillSystemTimer(i) == 0);
    assert(SYS_NbTimers == 0 && stops == 1);
    puts("SYSTEM.DRV Win16 drive/timer logic passed");
    return 0;
}
"""


class SystemDrv16Tests(unittest.TestCase):
    def test_real_source_logic(self):
        compiler = os.environ.get("CC", "cc")
        if not shutil.which(compiler):
            self.skipTest("C compiler not installed")
        source = SOURCE.read_text(encoding="utf-8")
        functions = "\n\n".join(
            function_body(source, name)
            for name in ("InquireSystem16", "CreateSystemTimer16", "SYSTEM_KillSystemTimer")
        )
        code = HARNESS.replace("__SYSTEM_FUNCTIONS__", functions)
        with tempfile.TemporaryDirectory(prefix="water-system16-") as dirname:
            root = Path(dirname)
            source_path = root / "system_test.c"
            target = root / "system_test"
            source_path.write_text(code, encoding="utf-8")
            build = subprocess.run(
                [compiler, "-std=c99", "-Wall", "-Wextra", "-Werror",
                 str(source_path), "-o", str(target)],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            self.assertEqual(build.returncode, 0, build.stdout + build.stderr)
            run = subprocess.run(
                [str(target)], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
            )
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn("Win16 drive/timer logic passed", run.stdout)

    def test_abi_ordinals_unchanged(self):
        path = SOURCE.with_name("system.drv16.spec")
        text = path.read_text(encoding="utf-8")
        names = dict((int(ordinal), name) for ordinal, name in
                     re.findall(r"(?m)^(\d+)\s+pascal(?:\s+-ret16)?\s+(\w+)\(", text))
        expected = {
            1: "InquireSystem", 2: "CreateSystemTimer", 3: "KillSystemTimer",
            4: "EnableSystemTimers", 5: "DisableSystemTimers",
            6: "GetSystemMSecCount", 7: "Get80x87SaveSize",
            8: "Save80x87State", 9: "Restore80x87State", 20: "A20_Proc",
        }
        self.assertEqual(names, expected)
        self.assertIn("13 stub INQUIRELONGINTS", text)


if __name__ == "__main__":
    unittest.main()
