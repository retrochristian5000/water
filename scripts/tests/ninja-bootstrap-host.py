#!/usr/bin/env python3
# Regression for the host-only Ninja compiler/bootstrap boundary.
# This runs without checking out Ninja, building WATER, or touching gitlinks.
import importlib.util
import os
import pathlib
import shlex
import shutil
import subprocess
import tempfile
import unittest
from contextlib import ExitStack
from unittest import mock


ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location(
    'water_ninja_bootstrap', ROOT / 'scripts' / 'ensure-ninja.py'
)
NINJA = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(NINJA)


class NinjaHostBootstrap(unittest.TestCase):
    def setUp(self):
        # Host-side override variables must not bleed into deterministic tests.
        clean = mock.patch.dict(os.environ, {'CXX_FOR_BUILD': '', 'AR_FOR_BUILD': ''})
        clean.start()
        self.addCleanup(clean.stop)
        self.tmp = tempfile.TemporaryDirectory(prefix='water ninja host ')
        self.addCleanup(self.tmp.cleanup)
        self.root = pathlib.Path(self.tmp.name)
        self.cxx = next(
            (shutil.which(name) for name in ('c++', 'clang++', 'g++')
             if shutil.which(name)),
            None,
        )
        if not self.cxx:
            self.skipTest('no host C++ compiler')

    def compiler_alias(self):
        target = self.root / 'compiler with spaces' / 'c++'
        target.parent.mkdir(exist_ok=True)
        target.symlink_to(self.cxx)
        return target

    def test_compiler_path_with_spaces_is_one_argument(self):
        alias = self.compiler_alias()
        cmd = NINJA.host_shell_command(str(alias))
        self.assertEqual(shlex.split(cmd), [str(alias)])
        self.assertEqual(NINJA.host_command_argv(str(alias)), [str(alias)])
        self.assertTrue(NINJA.compiler_version(cmd))

    def test_compiler_flags_are_sanitized_and_probe_runs(self):
        alias = self.compiler_alias()
        cmd = NINJA.host_shell_command(str(alias))
        ambient = dict(os.environ)
        ambient.pop('AR_FOR_BUILD', None)
        ambient.update({
            'CXXFLAGS': '--target=aarch64-pc-windows-msvc',
            'CPPFLAGS': '-DPOISONED_PE_BOOTSTRAP=1',
            'LDFLAGS': '-not-a-real-host-linker-flag',
        })
        with mock.patch.dict(os.environ, ambient, clear=True):
            env = NINJA.bootstrap_environment(cmd, '')
        self.assertEqual(shlex.split(env['CXX']), [str(alias)])
        for forbidden in ('CXXFLAGS', 'CPPFLAGS', 'LDFLAGS'):
            self.assertNotIn(forbidden, env)
        NINJA.validate_native_cxx(cmd, env)

    def test_compiler_cache_and_archive_path_quote(self):
        cxx = self.compiler_alias()
        cache = self.root / 'cache with spaces' / 'ccache'
        cache.parent.mkdir(exist_ok=True)
        cache.symlink_to(self.cxx)
        ar = self.root / 'archive tools with spaces' / 'ar'
        ar.parent.mkdir(exist_ok=True)
        ar.symlink_to(shutil.which('ar') or '/bin/true')
        with mock.patch.dict(os.environ, {'AR_FOR_BUILD': str(ar)}):
            env = NINJA.bootstrap_environment(str(cxx), '', str(cache))
        self.assertEqual(shlex.split(env['CXX']), [str(cache), str(cxx)])
        self.assertEqual(shlex.split(env['AR']), [str(ar)])

    def test_version_banner_does_not_bypass_native_compile(self):
        fake = self.root / 'version-only-cxx'
        fake.write_text(
            '#!/bin/sh\n'
            'if [ "${1:-}" = "--version" ]; then echo "GNU C++ mock"; exit 0; fi\n'
            'exit 17\n',
            encoding='utf-8',
        )
        fake.chmod(0o755)
        self.assertIn('GNU C++ mock', NINJA.compiler_version(str(fake)))
        env = NINJA.bootstrap_environment(str(fake), '')
        with self.assertRaisesRegex(RuntimeError, 'native Ninja compile/link probe'):
            NINJA.validate_native_cxx(str(fake), env)

    def test_inherited_target_cxx_does_not_shadow_host_compiler(self):
        with mock.patch.object(NINJA.platform, 'system', return_value='Linux'):
            with mock.patch.dict(os.environ, {'CXX': '/missing/windows-pe/clang++'},
                                 clear=False):
                chosen = NINJA.select_host_cxx()
        self.assertIn(pathlib.Path(chosen).name, ('c++', 'clang++', 'g++'))

    def test_out_of_tree_bootstrap_reuses_healthy_cache(self):
        fake_source = self.root / 'ninja source'
        fake_source.mkdir()
        (fake_source / 'configure.py').write_text(
            "from pathlib import Path\n"
            "p = Path('ninja')\n"
            "p.write_text('#!/bin/sh\\n[ \"${1:-}\" = --version ] && echo 1.13.0\\n')\n"
            "p.chmod(0o755)\n",
            encoding='utf-8',
        )
        alias = self.compiler_alias()
        with ExitStack() as stack:
            stack.enter_context(mock.patch.object(
                NINJA, 'ensure_ninja_source', return_value='mock-commit'))
            stack.enter_context(mock.patch.object(NINJA, 'SUBMODULE_DIR', fake_source))
            stack.enter_context(mock.patch.object(
                NINJA, 'select_host_cxx', return_value=str(alias)))
            stack.enter_context(mock.patch.object(NINJA, 'select_host_sdkroot', return_value=''))
            stack.enter_context(mock.patch.object(NINJA, 'select_host_macos_arch', return_value=''))
            stack.enter_context(mock.patch.object(NINJA, 'select_compiler_cache', return_value=''))
            binary = NINJA.ensure_bundled_ninja(self.root / 'water-build')
            self.assertTrue(NINJA.binary_is_usable(binary))
            self.assertTrue((binary.parent / '.whp-ninja-tool').is_file())
            with mock.patch.object(NINJA, 'copy_ninja_source',
                                   side_effect=AssertionError('cached Ninja was rebuilt')):
                self.assertEqual(
                    binary, NINJA.ensure_bundled_ninja(self.root / 'water-build')
                )
        self.assertFalse(list((self.root / '.whp-host-tools').glob('*.new.*')))

    def test_failed_staged_bootstrap_does_not_replace_cache(self):
        fake_source = self.root / 'broken ninja source'
        fake_source.mkdir()
        (fake_source / 'configure.py').write_text('raise SystemExit(17)\n',
                                                   encoding='utf-8')
        alias = self.compiler_alias()
        with ExitStack() as stack:
            stack.enter_context(mock.patch.object(
                NINJA, 'ensure_ninja_source', return_value='broken-mock'))
            stack.enter_context(mock.patch.object(NINJA, 'SUBMODULE_DIR', fake_source))
            stack.enter_context(mock.patch.object(
                NINJA, 'select_host_cxx', return_value=str(alias)))
            stack.enter_context(mock.patch.object(NINJA, 'select_host_sdkroot', return_value=''))
            stack.enter_context(mock.patch.object(NINJA, 'select_host_macos_arch', return_value=''))
            stack.enter_context(mock.patch.object(NINJA, 'select_compiler_cache', return_value=''))
            with self.assertRaises(subprocess.CalledProcessError):
                NINJA.ensure_bundled_ninja(self.root / 'water-build')
        tool_dir = self.root / '.whp-host-tools'
        self.assertFalse(list(tool_dir.glob('*.new.*')))
        self.assertFalse(list(tool_dir.glob('*/.whp-ninja-tool')))

    def test_invalid_compiler_stops_before_any_build(self):
        with self.assertRaisesRegex(RuntimeError, 'not executable'):
            NINJA.host_shell_command(str(self.root / 'absent compiler'))


if __name__ == '__main__':
    unittest.main()
