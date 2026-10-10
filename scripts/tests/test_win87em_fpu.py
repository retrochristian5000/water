#!/usr/bin/env python3
"""Compile Water's private x87 helper and exercise real CPU rounding.

No Windows guest is booted and no native WIN87EM.DLL binary is executed.
"""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FPU = ROOT / "dlls/win87em.dll16/win87em_fpu.h"
CFILE = ROOT / "dlls/win87em.dll16/win87em.c"
SPEC = ROOT / "dlls/win87em.dll16/win87em.dll16.spec"

HARNESS = r"""
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "win87em_fpu.h"

#if defined(__i386__) || defined(__x86_64__)

static uint16_t control_word(void)
{
    uint16_t cw;
    __asm__ __volatile__("fnstcw %0" : "=m"(cw));
    return cw;
}

static void push(double number)
{
    __asm__ __volatile__("fldl %0" : : "m"(number));
}

static double pop(void)
{
    double number;
    __asm__ __volatile__("fstpl %0" : "=m"(number));
    return number;
}

int main(void)
{
    uint16_t original = control_word();
    int32_t result = 0;
    double value;

    value = 1.75;
    push(value);
    assert(win87em_round_st0(0x0400)); /* round down */
    assert(pop() == 1.0);
    assert(control_word() == original);

    value = -1.75;
    push(value);
    assert(win87em_round_st0(0x0800)); /* round up */
    assert(pop() == -1.0);
    assert(control_word() == original);

    value = 3.75;
    push(value);
    assert(win87em_pop_int32(0x0c00, &result)); /* truncate */
    assert(result == 3);
    assert(control_word() == original);

    value = -19.9;
    push(value);
    assert(win87em_pop_int32(0x0c00, &result));
    assert(result == -19);
    assert(control_word() == original);

    value = -19.9;
    push(value);
    assert(win87em_pop_int32(0x0400, &result)); /* down */
    assert(result == -20);
    assert(control_word() == original);

    /* Null output must not pop or change the FPU control word. */
    value = 42.0;
    push(value);
    assert(!win87em_pop_int32(0, NULL));
    assert(pop() == 42.0);
    assert(control_word() == original);

    puts("win87em host x87 rounding and pop: passed");
    return 0;
}
#else
int main(void)
{
    int32_t value = 123;
    assert(!win87em_round_st0(0));
    assert(!win87em_pop_int32(0, &value));
    assert(value == 123);
    puts("win87em non-x86 host fallback: passed");
    return 0;
}
#endif
"""


class Win87EmTests(unittest.TestCase):
    def test_native_fpu_rounding_and_pop(self):
        compiler = shlex.split(os.environ.get("CC", "cc"))
        if not compiler or not shutil.which(compiler[0]):
            self.skipTest("no host C compiler")
        with tempfile.TemporaryDirectory(prefix="water-win87em-") as tmp:
            code = Path(tmp) / "test.c"
            program = Path(tmp) / "win87em_test"
            code.write_text(HARNESS, encoding="utf-8")
            cmd = compiler + ["-std=c99", "-Wall", "-Wextra", "-Werror",
                              "-I", str(FPU.parent), str(code), "-o", str(program)]
            built = subprocess.run(cmd, capture_output=True, text=True)
            self.assertEqual(built.returncode, 0, built.stderr)
            checked = subprocess.run([str(program)], capture_output=True, text=True)
            self.assertEqual(checked.returncode, 0, checked.stderr)
            self.assertIn("win87em", checked.stdout)

    def test_win16_export_contract(self):
        spec = SPEC.read_text(encoding="utf-8")
        source = CFILE.read_text(encoding="utf-8")
        self.assertEqual(spec.splitlines(), [
            "1 pascal -register __fpMath()",
            "3 pascal -ret16 __WinEm87Info(ptr word) __WinEm87Info",
            "4 pascal -ret16 __WinEm87Restore(ptr word) __WinEm87Restore",
            "5 pascal -ret16 __WinEm87Save(ptr word) __WinEm87Save",
        ])
        for name in ("Info", "Save", "Restore"):
            self.assertRegex(source, r"WORD WINAPI __WinEm87" + name + r"\(")
        self.assertIn("win87em_round_st0(LOWORD(context->Eax))", source)
        self.assertIn("win87em_pop_int32(LOWORD(context->Eax), &value)", source)
        self.assertNotIn("stub !", source)


if __name__ == "__main__":
    unittest.main()
