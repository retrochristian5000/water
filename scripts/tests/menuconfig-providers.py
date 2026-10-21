#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Guard Python and POSIX menuconfig metadata and provider persistence."""
from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[2]
MENU = ROOT / "scripts" / "whp-config"
sys.path.insert(0, str(MENU))
import config as whp_config  # noqa: E402


def shell_schema():
    schema = {}
    for number, line in enumerate((MENU / "menu-options.def").read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("|")
        if len(fields) != 7:
            raise AssertionError(f"schema line {number} needs seven fields")
        key, section, label, kind, default, choices, group = fields
        if key in schema:
            raise AssertionError(f"duplicate shell schema option: {key}")
        schema[key] = (section, label, kind, default, tuple(filter(None, choices.split(","))), group)
    return schema


class MenuconfigProvidersTest(unittest.TestCase):
    def test_python_and_shell_schemas_match(self):
        expected = {
            option.key: (option.section, option.label, option.kind, option.default,
                         option.choices, option.group)
            for option in whp_config.OPTIONS
        }
        self.assertEqual(shell_schema(), expected)

    def test_defaults_and_valid_providers(self):
        expected = {
            "WATER_AUTOCONF_PROVIDER": ("pinned", "host", "cached"),
            "WATER_AUTOMAKE_PROVIDER": ("pinned", "host"),
            "WATER_AUTOMAKE_EXEC_PROVIDER": ("auto", "pinned", "host"),
        }
        for key, choices in expected.items():
            option = whp_config.OPTION_BY_KEY[key]
            self.assertEqual(option.default,
                             "auto" if key == "WATER_AUTOMAKE_EXEC_PROVIDER" else "pinned")
            self.assertEqual(option.choices, choices)
            for value in option.choices:
                whp_config.validate_value(option, value)
            with self.assertRaises(ValueError):
                whp_config.validate_value(option, "unsupported")

    def test_menu_round_trip_and_override_precedence(self):
        with tempfile.TemporaryDirectory(prefix="water-menuconfig-") as root:
            config_file = pathlib.Path(root) / ".whpconfig"
            state = whp_config.load_config(config_file)
            state.values["WATER_AUTOCONF_PROVIDER"] = "host"
            state.values["WATER_AUTOMAKE_PROVIDER"] = "host"
            state.values["WATER_AUTOMAKE_EXEC_PROVIDER"] = "pinned"
            whp_config.save_config(config_file, state)

            state = whp_config.load_config(config_file)
            self.assertEqual(state.values["WATER_AUTOCONF_PROVIDER"], "host")
            self.assertEqual(state.values["WATER_AUTOMAKE_PROVIDER"], "host")
            self.assertEqual(state.values["WATER_AUTOMAKE_EXEC_PROVIDER"], "pinned")
            assignments = whp_config.shell_assignments(state, {})
            self.assertIn("WATER_AUTOCONF_PROVIDER='host'", assignments)
            self.assertIn("WATER_AUTOMAKE_PROVIDER='host'", assignments)
            self.assertIn("WATER_AUTOMAKE_EXEC_PROVIDER='pinned'", assignments)
            # Environment variables take precedence over persistent profile values.
            assignments = whp_config.shell_assignments(
                state, {"WATER_AUTOCONF_PROVIDER": "pinned"})
            self.assertNotIn("WATER_AUTOCONF_PROVIDER=", assignments)
            self.assertIn("WATER_AUTOMAKE_PROVIDER='host'", assignments)

            for command in (
                [sys.executable, str(MENU / "menuconfig.py"), str(config_file), "--dump"],
                ["/bin/sh", str(MENU / "menuconfig.sh"), str(config_file), "--dump"],
            ):
                result = subprocess.run(command, capture_output=True, text=True, check=True)
                for key in ("WATER_AUTOCONF_PROVIDER", "WATER_AUTOMAKE_PROVIDER",
                            "WATER_AUTOMAKE_EXEC_PROVIDER"):
                    expected_value = "pinned" if key == "WATER_AUTOMAKE_EXEC_PROVIDER" else "host"
                    self.assertIn(expected_value, result.stdout)
                    self.assertIn(key if command[0] == "/bin/sh" else
                                  whp_config.OPTION_BY_KEY[key].label, result.stdout)


    def test_autotools_bootstrap_order_and_bypass(self):
        wrapper = (ROOT / "build.sh").read_text(encoding="utf-8")
        auto_fn = wrapper.index("prepare_automake_executables()")
        ac_fn = wrapper.index("prepare_autoconf_toolchain()")
        cache = wrapper.index('configure-cache.sh" check', ac_fn)
        prepare = wrapper.index("    prepare_automake_executables\n", ac_fn)
        build_autoconf = wrapper.index("ensure-autoconf.sh", prepare)
        self.assertLess(auto_fn, ac_fn)
        self.assertLess(cache, prepare)
        self.assertLess(prepare, build_autoconf)
        self.assertIn('if [ "$AUTOMAKE_EXEC_EXPLICIT" = 1 ]; then', wrapper)
        helper = ROOT / "scripts" / "whp-build" / "ensure-automake.sh"
        self.assertTrue(helper.is_file())
        self.assertIn("git clone -q --shared", helper.read_text(encoding="utf-8"))

    def test_maintenance_without_automake_bootstrap(self):
        """A host Automake selection must not block unrelated maintenance."""
        if os.name == "nt":
            self.skipTest("Water's build wrapper requires a POSIX shell")
        dirname = shutil.which("dirname")
        if not dirname:
            self.skipTest("dirname unavailable")

        with tempfile.TemporaryDirectory(prefix="water-automake-bypass-") as root:
            tmp = pathlib.Path(root)
            shutil.copy2(ROOT / "build.sh", tmp / "build.sh")
            tools = tmp / "tools"
            tools.mkdir()
            (tools / "dirname").symlink_to(dirname)
            env = os.environ.copy()
            env.update({
                "PATH": str(tools),
                "WHP_GIT_UPDATE": "0",
                "WHP_SUBMODULES": "0",
                "WATER_LLVM_LINKER": "system",
                "WATER_AUTOCONF_PROVIDER": "cached",
                "WATER_AUTOMAKE_PROVIDER": "host",
                "WATER_BASH_BOOTSTRAP": "n",
            })
            env.pop("WHP_AUTOMAKE_SOURCE_DIR", None)
            result = subprocess.run(["/bin/sh", str(tmp / "build.sh"), "clean"],
                                    env=env, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertIn("already absent", result.stderr)


if __name__ == "__main__":
    unittest.main()
