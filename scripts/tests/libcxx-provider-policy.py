#!/usr/bin/env python3
"""Test real Water libc++ provider-selection functions without a full build.

Extracts unchanged shell functions from build.sh, stubs *only* the actual
provider compilation, and runs the selector across explicit architectures.
A successful policy check does not validate runtime ABI/linker support.
"""
import os
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
build = (root / "build.sh").read_text(encoding="utf-8")


def get_function(name):
    pattern = rf"(?m)^{re.escape(name)}\(\)\n\{{\n.*?^\}}"
    matches = re.findall(pattern, build, re.DOTALL | re.MULTILINE)
    if len(matches) != 1:
        raise AssertionError(f"expected exactly one {name} function")
    return matches[0]


source = "\n\n".join([
    "die() { printf '%s\\n' \"$*\" >&2; exit 1; }",
    get_function("selected_libcxx_archs"),
    # The test does not build any provider or inspect a cached runtime.
    "selected_llvm_libcxx_archs() { :; }",
    get_function("prepare_libcxx_provider"),
    "prepare_libcxx_provider",
    "printf 'LEGACY=%s\\nSTATE=%s\\n' \"$WHP_LIBCXX_INSTALL_LEGACY_HEADERS\" \"$WHP_LIBCXX_STATE\"",
])


def test(arch, policy, i386="legacy", mode="llvm", expected_ok=False, marker=""):
    env = os.environ.copy()
    env.update(
        WHP_CONFIGURE_ARCHS_SET="1",
        WHP_CONFIGURE_ARCHS=arch,
        WATER_ARCHS_MODE="custom",
        WATER_LIBCXX=mode,
        WATER_LIBCXX_I386=i386,
        WATER_LIBCXX_LEGACY_POLICY=policy,
        LLVM_LIBCXX_RUNTIME_DIR="/nonexistent-test-only",
        WHP_BASH_CMD="",
    )
    proc = subprocess.run(
        ["/bin/sh", "-c", source], env=env, text=True, capture_output=True
    )
    if (proc.returncode == 0) != expected_ok:
        raise AssertionError(
            f"{arch}/{policy}/{i386}/{mode}: "
            f"unexpected status {proc.returncode}\n{proc.stdout}\n{proc.stderr}"
        )
    output = proc.stdout + proc.stderr
    if marker and marker not in output:
        raise AssertionError(
            f"{arch}/{policy}/{i386}/{mode}: expected {marker!r}\n{output}"
        )
    return proc


def main():
    test("powerpc", "allow", expected_ok=True, marker="LEGACY=1")
    test("powerpc", "error", marker="PowerPC Windows PE/COFF C++ runtime is not ready")
    test("arm", "allow", expected_ok=True, marker="LEGACY=1")
    test("arm", "error", marker="ARM32 PE lacks a validated LLVM provider")
    test("i386", "allow", expected_ok=True, marker="LEGACY=1")
    test("i386", "error", marker="i386 requires WATER_LIBCXX_I386=llvm")
    test("i386", "error", i386="llvm", expected_ok=True, marker="LEGACY=0")
    test("x86_64,aarch64,arm64ec", "error", expected_ok=True, marker="LEGACY=0")
    test("none", "error", expected_ok=True, marker="LEGACY=0")
    test("x86_64", "error", mode="legacy",
         marker="rejects WATER_LIBCXX=legacy")
    # Policy changes must appear in the state token used for reconfigure.
    allowed = test("powerpc", "allow", expected_ok=True, marker="legacy-policy:allow")
    modern = test("x86_64", "error", expected_ok=True, marker="legacy-policy:error")
    assert "LEGACY=1" in allowed.stdout
    assert "LEGACY=0" in modern.stdout
    # Confirm the real build script explicitly rejects an unverified
    # user override in strict mode rather than trusting arbitrary flags.
    assert 'strict modern libc++ policy rejects unverified' in build
    print("PASS: Water libc++ strict/legacy provider routing (source functions, no build)")


if __name__ == "__main__":
    try:
        main()
    except (AssertionError, OSError) as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
