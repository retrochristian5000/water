#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

from __future__ import annotations

import argparse
import hashlib
import os
import pathlib
import platform
import shlex
import shutil
import subprocess
import sys
import tempfile
from typing import List

ROOT = pathlib.Path(__file__).resolve().parents[1]
SUBMODULE_REL = pathlib.Path('toolchains/ninja-builder')
SUBMODULE_DIR = ROOT / SUBMODULE_REL
NINJA_BOOTSTRAP_SCHEMA='6'
GIT_BIN = os.environ.get('WHP_GIT_SEED') or 'git'


def run_text(command: List[str], cwd: pathlib.Path | None = None) -> str:
    completed = subprocess.run(
        command,
        cwd=cwd,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise RuntimeError(f"command failed: {' '.join(command)}{': ' + detail if detail else ''}")
    return completed.stdout.strip()


def git_checkout_available() -> bool:
    return shutil.which(GIT_BIN) is not None and (ROOT / '.git').exists()


def archive_source_signature() -> str:
    digest = hashlib.sha256()
    if not (SUBMODULE_DIR / 'configure.py').is_file():
        raise RuntimeError(
            f'bundled Ninja source is unavailable: {SUBMODULE_DIR}; '
            'initialize toolchains/ninja-builder or use a Git checkout'
        )
    for path in sorted(SUBMODULE_DIR.rglob('*')):
        if not path.is_file() or '.git' in path.parts or 'build' in path.parts:
            continue
        digest.update(str(path.relative_to(SUBMODULE_DIR)).encode('utf-8'))
        digest.update(b'\0')
        digest.update(path.read_bytes())
        digest.update(b'\0')
    return f'archive-{digest.hexdigest()}'


def ensure_ninja_source() -> str:
    if not git_checkout_available():
        return archive_source_signature()

    expected_line = run_text(
        [GIT_BIN, '-C', str(ROOT), 'ls-tree', 'HEAD', '--', str(SUBMODULE_REL)]
    )
    fields = expected_line.split()
    if len(fields) < 3 or fields[1] != 'commit':
        raise RuntimeError(f'Ninja gitlink is not registered in Water: {SUBMODULE_REL}')
    expected_revision = fields[2]

    current_revision = ''
    if (SUBMODULE_DIR / '.git').exists():
        try:
            current_revision = run_text([GIT_BIN, '-C', str(SUBMODULE_DIR), 'rev-parse', 'HEAD'])
        except RuntimeError:
            current_revision = ''

    if current_revision != expected_revision:
        if os.environ.get('WHP_SUBMODULES', '1') != '0':
            subprocess.run(
                [
                    '/bin/sh', str(ROOT / 'scripts/whp-build/ensure-submodule.sh'),
                    str(ROOT), str(SUBMODULE_REL),
                ],
                stdout=sys.stderr,
                stderr=sys.stderr,
                check=True,
            )
            current_revision = run_text(
                [GIT_BIN, '-C', str(SUBMODULE_DIR), 'rev-parse', 'HEAD']
            )
        elif not current_revision:
            raise RuntimeError(
                f'bundled Ninja source is not initialized and WHP_SUBMODULES=0: {SUBMODULE_DIR}'
            )

    if current_revision != expected_revision:
        ancestry = subprocess.run(
            [GIT_BIN, '-C', str(SUBMODULE_DIR), 'merge-base', '--is-ancestor',
             expected_revision, current_revision],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        if ancestry.returncode != 0:
            raise RuntimeError(
                'bundled Ninja checkout is not provably newer than the Water gitlink; '
                f'preserving {current_revision} (pin {expected_revision}); '
                'repair source history or advance the parent gitlink explicitly'
            )
        print(
            f'WATER Ninja: using newer local source {current_revision} '
            f'(parent pin {expected_revision} retained)',
            file=sys.stderr,
        )

    dirty = run_text(
        [GIT_BIN, '-C', str(SUBMODULE_DIR), 'status', '--porcelain', '--untracked-files=no']
    )
    if dirty:
        raise RuntimeError(
            'bundled Ninja submodule has tracked changes; commit them in the Ninja fork '
            'and update the Water gitlink'
        )
    return current_revision


def host_command_argv(command: str) -> List[str]:
    """Preserve a pathname containing spaces; parse intentional argv otherwise."""
    if not command:
        raise RuntimeError('empty host tool command')
    if pathlib.Path(command).is_file():
        argv = [command]
    else:
        try:
            argv = shlex.split(command)
        except ValueError as exc:
            raise RuntimeError(f'invalid host tool command: {command}') from exc
    if not argv:
        raise RuntimeError('empty host tool command')
    executable = shutil.which(argv[0])
    if not executable or not os.path.isfile(executable) or not os.access(executable, os.X_OK):
        raise RuntimeError(f'host tool is not executable: {argv[0]}')
    argv[0] = executable
    return argv


def host_shell_command(command: str) -> str:
    """Ninja's configure.py runs CXX and AR through a shell."""
    return ' '.join(shlex.quote(arg) for arg in host_command_argv(command))


def select_host_cxx() -> str:
    requested = os.environ.get('CXX_FOR_BUILD')
    if requested:
        return requested

    # Ninja executes on the build machine, so on macOS it must not inherit
    # Water's target CXX. A self-built Clang targeting aarch64-apple-darwin does
    # not necessarily carry an Apple SDK sysroot and can fail through libc++
    # with headers such as <errno.h> missing. Prefer the SDK compiler for the
    # build-machine role unless CXX_FOR_BUILD explicitly overrides it.
    if platform.system() == 'Darwin' and shutil.which('xcrun'):
        candidate = run_text(['xcrun', '--sdk', 'macosx', '--find', 'clang++'])
        if candidate:
            return candidate

    # WATER's CXX may target Windows PE; do not treat it as a native host
    # compiler when an ordinary host compiler is available.
    for name in ('c++', 'clang++', 'g++'):
        path = shutil.which(name)
        if path:
            return path

    # Retain an explicit CXX fallback on unusual systems with no host c++ name.
    requested = os.environ.get('CXX')
    if requested:
        return requested
    raise RuntimeError('a native C++17 compiler is required (set CXX_FOR_BUILD to override)')


def select_host_sdkroot() -> str:
    if platform.system() != 'Darwin':
        return ''
    if not shutil.which('xcrun'):
        raise RuntimeError('xcrun is required to locate the macOS SDK for bundled Ninja')
    sdkroot = run_text(['xcrun', '--sdk', 'macosx', '--show-sdk-path'])
    if not sdkroot:
        raise RuntimeError('xcrun did not return a macOS SDK path for bundled Ninja')
    return sdkroot


def compiler_supports_macos_arch(cxx: str, sdkroot: str, arch: str) -> bool:
    if platform.system() != 'Darwin' or not sdkroot:
        return False
    argv = host_command_argv(cxx)
    if not argv:
        return False

    with tempfile.TemporaryDirectory(prefix='whp-ninja-arch-') as tmp:
        output = pathlib.Path(tmp) / 'probe'
        completed = subprocess.run(
            [
                *argv,
                '-arch', arch,
                '-isysroot', sdkroot,
                '-x', 'c++', '-',
                '-o', str(output),
            ],
            input='int main() { return 0; }\n',
            text=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
        if completed.returncode != 0 or not output.is_file():
            return False
        # A bootstrap architecture is useful only if the host can execute it.
        # macOS 15 can compile/link arm64e user programs but intentionally
        # rejects them at launch; macOS 26 supports third-party arm64e directly.
        return binary_is_usable(output)


def select_host_macos_arch(cxx: str, sdkroot: str) -> str:
    if platform.system() != 'Darwin':
        return ''

    requested = os.environ.get('NINJA_MACOS_ARCH', 'auto')
    if requested not in ('auto', 'arm64', 'arm64e', 'x86_64'):
        raise RuntimeError(
            'NINJA_MACOS_ARCH must be auto, arm64, arm64e, or x86_64: '
            f'{requested}'
        )

    machine = platform.machine().lower()
    if machine in ('arm64', 'aarch64'):
        if requested == 'auto':
            if compiler_supports_macos_arch(cxx, sdkroot, 'arm64e'):
                return 'arm64e'
            return 'arm64'
        if requested == 'arm64':
            return 'arm64'
        if requested == 'arm64e':
            if not compiler_supports_macos_arch(cxx, sdkroot, 'arm64e'):
                raise RuntimeError(
                    'requested Ninja arm64e bootstrap cannot be compiled, linked, '
                    'and executed by the selected compiler, SDK, and host macOS'
                )
            return 'arm64e'
        raise RuntimeError(
            f'requested Ninja macOS architecture is not native to Apple Silicon: {requested}'
        )

    if machine in ('x86_64', 'amd64'):
        if requested in ('auto', 'x86_64'):
            return 'x86_64'
        raise RuntimeError(
            f'requested Ninja macOS architecture is not native to x86_64: {requested}'
        )

    raise RuntimeError(f'unsupported macOS host architecture for Ninja bootstrap: {machine}')


def select_compiler_cache() -> str:
    explicit = os.environ.get('WHP_COMPILER_CACHE_CMD', '')
    if explicit:
        argv = [explicit] if pathlib.Path(explicit).is_file() else shlex.split(explicit)
        if len(argv) != 1:
            raise RuntimeError('WHP_COMPILER_CACHE_CMD must name exactly one executable')
        candidate = argv[0]
        path = candidate if pathlib.Path(candidate).is_absolute() else shutil.which(candidate)
        if not path or not os.path.isfile(path) or not os.access(path, os.X_OK):
            raise RuntimeError(f'compiler cache is not executable: {candidate}')
        if pathlib.Path(path).name not in ('ccache', 'sccache'):
            raise RuntimeError(f'unsupported compiler cache command: {path}')
        return str(path)

    policy = os.environ.get('WATER_COMPILER_CACHE', os.environ.get('COMPILER_CACHE', 'auto'))
    if policy not in ('auto', 'ccache', 'sccache', 'none'):
        raise RuntimeError(
            f'WATER_COMPILER_CACHE must be auto, ccache, sccache, or none: {policy}'
        )
    if policy == 'none':
        return ''

    names = ('sccache', 'ccache') if policy == 'auto' else (policy,)
    for name in names:
        path = shutil.which(name)
        if path:
            return path
    if policy != 'auto':
        raise RuntimeError(f'requested compiler cache is not installed: {policy}')
    return ''


def compiler_version(command: str) -> str:
    argv = host_command_argv(command)
    completed = subprocess.run(
        [*argv, '--version'],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    first = completed.stdout.splitlines()[0] if completed.stdout else ''
    if completed.returncode != 0 or not first:
        raise RuntimeError(f'could not identify host C++ compiler: {command}')
    return first


def validate_native_cxx(command: str, bootstrap_env: dict[str, str]) -> None:
    """A version banner cannot prove a compiler produces runnable host tools."""
    argv = host_command_argv(command)
    flags = shlex.split(bootstrap_env.get('CXXFLAGS', ''))
    ldflags = shlex.split(bootstrap_env.get('LDFLAGS', ''))
    with tempfile.TemporaryDirectory(prefix='whp-ninja-cxx-') as tmp:
        source = pathlib.Path(tmp) / 'native.cc'
        binary = pathlib.Path(tmp) / 'native'
        source.write_text(
            '#include <vector>\n'
            '#include <type_traits>\n'
            'int main() { if constexpr (std::is_same_v<int, int>) '
            'return std::vector<int>{1}.front() - 1; return 1; }\n',
            encoding='utf-8',
        )
        compiled = subprocess.run(
            [*argv, *flags, '-std=c++17', str(source), *ldflags, '-o', str(binary)],
            env=bootstrap_env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if compiled.returncode != 0 or not binary.is_file():
            message = (compiled.stderr or compiled.stdout).strip()
            raise RuntimeError(
                'host C++17 compiler failed native Ninja compile/link probe: '
                f'{command}' + (f' ({message[-900:]})' if message else '')
            )
        try:
            launched = subprocess.run(
                [str(binary)],
                env=bootstrap_env,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=False,
            )
        except OSError as exc:
            raise RuntimeError(
                'host C++17 compiler generated a non-executable host binary '
                f'(possible PE target or architecture mismatch): {command}'
            ) from exc
        if launched.returncode != 0:
            raise RuntimeError(
                'host C++17 compiler generated a binary that cannot run '
                f'on this host (exit {launched.returncode}): {command}'
            )


def marker_text(
    revision: str,
    cxx: str,
    cxx_version: str,
    sdkroot: str,
    macos_arch: str = '',
) -> str:
    return (
        f'NINJA_BOOTSTRAP_SCHEMA={NINJA_BOOTSTRAP_SCHEMA}\n'
        f'SOURCE_DIR={ROOT}\n'
        f'NINJA_GIT_COMMIT={revision}\n'
        f'HOST_SYSTEM={platform.system()}\n'
        f'HOST_MACHINE={platform.machine()}\n'
        f'PYTHON={pathlib.Path(sys.executable).resolve()}\n'
        f'CXX={cxx}\n'
        f'CXX_VERSION={cxx_version}\n'
        f'SDKROOT={sdkroot}\n'
        f'NINJA_MACOS_ARCH={macos_arch}\n'
    )


def ninja_binary(directory: pathlib.Path) -> pathlib.Path:
    for name in ('ninja', 'ninja.exe'):
        candidate = directory / name
        if candidate.is_file():
            return candidate
    return directory / ('ninja.exe' if os.name == 'nt' else 'ninja')


def binary_is_usable(path: pathlib.Path) -> bool:
    if not path.is_file():
        return False
    try:
        completed = subprocess.run(
            [str(path), '--version'],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            check=False,
        )
    except OSError:
        return False
    return completed.returncode == 0


def binary_has_macos_arch(path: pathlib.Path, arch: str) -> bool:
    if platform.system() != 'Darwin' or not arch:
        return True
    lipo = shutil.which('lipo')
    if not lipo:
        return False
    completed = subprocess.run(
        [lipo, '-archs', str(path)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return completed.returncode == 0 and arch in completed.stdout.split()


def atomic_install(staging: pathlib.Path, final: pathlib.Path) -> None:
    backup = final.with_name(final.name + f'.old.{os.getpid()}')
    shutil.rmtree(backup, ignore_errors=True)
    if final.exists():
        final.rename(backup)
    try:
        staging.rename(final)
    except Exception:
        if not final.exists() and backup.exists():
            backup.rename(final)
        raise
    shutil.rmtree(backup, ignore_errors=True)


def copy_ninja_source(destination: pathlib.Path) -> None:
    # configure.py can regenerate lexer/parser sources when re2c is installed.
    # Build from an isolated source copy so optional host tools can never dirty
    # the pinned Ninja submodule.
    shutil.copytree(
        SUBMODULE_DIR,
        destination,
        symlinks=True,
        ignore=shutil.ignore_patterns('.git', 'build', 'build.ninja', 'ninja', 'ninja.exe'),
    )


def bootstrap_environment(
    cxx: str,
    sdkroot: str,
    compiler_cache: str = '',
    compiler_cache_dir: pathlib.Path | None = None,
    macos_arch: str = '',
) -> dict[str, str]:
    bootstrap_env = os.environ.copy()
    # Ninja is a host build helper. Do not let ambient target/search-path state
    # choose headers, libraries, architectures, or CMake/pkg-config prefixes
    # before the platform wrapper has a chance to sanitize the main Water build.
    # Explicit host compiler, SDK, cache and archive-tool inputs are restored
    # below after this isolation boundary.
    for key in (
        'CC', 'CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'AR', 'SDKROOT',
        'CCACHE_DIR', 'SCCACHE_DIR',
        'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'OBJC_INCLUDE_PATH',
        'COMPILER_PATH', 'GCC_EXEC_PREFIX', 'LIBRARY_PATH',
        'DYLD_LIBRARY_PATH', 'DYLD_FALLBACK_LIBRARY_PATH',
        'DYLD_INSERT_LIBRARIES', 'CMAKE_PREFIX_PATH', 'CMAKE_LIBRARY_PATH',
        'CMAKE_INCLUDE_PATH', 'PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR',
        'PKG_CONFIG_SYSROOT_DIR', 'ACLOCAL_PATH', 'ARCHFLAGS',
    ):
        bootstrap_env.pop(key, None)

    bootstrap_env['CXX'] = host_shell_command(cxx)
    if compiler_cache:
        cache_name = pathlib.Path(compiler_cache).name
        if cache_name not in ('ccache', 'sccache'):
            raise RuntimeError(f'unsupported compiler cache command: {compiler_cache}')
        bootstrap_env['CXX'] = f'{shlex.quote(compiler_cache)} {host_shell_command(cxx)}'
        if compiler_cache_dir is not None:
            if cache_name == 'ccache':
                bootstrap_env['CCACHE_DIR'] = str(compiler_cache_dir)
            else:
                bootstrap_env['SCCACHE_DIR'] = str(compiler_cache_dir)

    if sdkroot:
        flags = []
        if platform.system() == 'Darwin' and macos_arch:
            flags.extend(('-arch', macos_arch))
        flags.extend(('-isysroot', shlex.quote(sdkroot)))
        common_flags = ' '.join(flags)
        bootstrap_env['SDKROOT'] = sdkroot
        bootstrap_env['CXXFLAGS'] = common_flags
        link_flags = common_flags
        if platform.system() == 'Darwin':
            # Ninja is a build helper, not a Water deliverable. Optimize and
            # dead-strip the helper itself without leaking these flags into
            # Water or cross-toolchain link commands.
            link_flags += ' -Wl,-O2 -Wl,-dead_strip'
        bootstrap_env['LDFLAGS'] = link_flags
    if os.environ.get('AR_FOR_BUILD'):
        bootstrap_env['AR'] = host_shell_command(os.environ['AR_FOR_BUILD'])
    return bootstrap_env


def ensure_bundled_ninja(water_build_dir: pathlib.Path) -> pathlib.Path:
    revision = ensure_ninja_source()
    cxx = host_shell_command(select_host_cxx())
    sdkroot = select_host_sdkroot()
    macos_arch = select_host_macos_arch(cxx, sdkroot)
    compiler_cache = select_compiler_cache()
    cxx_version = compiler_version(cxx)
    expected_marker = marker_text(revision, cxx, cxx_version, sdkroot, macos_arch)

    host_tag = f'{platform.system().lower()}-{platform.machine().lower()}'
    host_tools_root = water_build_dir.parent / '.whp-host-tools'
    final_dir = host_tools_root / f'ninja-{host_tag}'
    marker = final_dir / '.whp-ninja-tool'
    binary = ninja_binary(final_dir)

    if (
        marker.is_file()
        and marker.read_text(encoding='utf-8') == expected_marker
        and binary_is_usable(binary)
        and binary_has_macos_arch(binary, macos_arch)
    ):
        print(f'Reused bundled Ninja: {binary}', file=sys.stderr)
        return binary.resolve()

    compiler_cache_dir = None
    if compiler_cache:
        cache_name = pathlib.Path(compiler_cache).name
        compiler_cache_dir = (
            water_build_dir.parent / '.whp-compiler-cache' / f'{cache_name}-{host_tag}'
        )
        compiler_cache_dir.mkdir(parents=True, exist_ok=True)
    bootstrap_env = bootstrap_environment(
        cxx,
        sdkroot,
        compiler_cache,
        compiler_cache_dir,
        macos_arch,
    )
    # Validate the native C++ toolchain before copying/building the Ninja
    # source.  This catches Windows-targeting compilers and missing C++ headers.
    validate_native_cxx(cxx, bootstrap_env)

    host_tools_root.mkdir(parents=True, exist_ok=True)
    staging = pathlib.Path(tempfile.mkdtemp(
        prefix=f'{final_dir.name}.new.', dir=str(host_tools_root),
    ))
    staged_source = staging / 'source'
    bootstrap_dir = staging / 'bootstrap'

    print(
        f'Bootstrapping bundled Ninja {revision[:12]} with {cxx_version}',
        file=sys.stderr,
    )
    if sdkroot:
        print(f'Bundled Ninja macOS SDK: {sdkroot}', file=sys.stderr)
    if macos_arch:
        print(f'Bundled Ninja macOS arch: {macos_arch}', file=sys.stderr)
    if compiler_cache and compiler_cache_dir is not None:
        print(
            f'Bundled Ninja compiler cache: {compiler_cache} ({compiler_cache_dir})',
            file=sys.stderr,
        )
    try:
        copy_ninja_source(staged_source)
        bootstrap_dir.mkdir()
        subprocess.run(
            [sys.executable, str(staged_source / 'configure.py'), '--bootstrap'],
            cwd=bootstrap_dir,
            env=bootstrap_env,
            stdout=sys.stderr,
            stderr=sys.stderr,
            check=True,
        )
        staged_binary = ninja_binary(bootstrap_dir)
        if not binary_is_usable(staged_binary):
            raise RuntimeError(f'bundled Ninja bootstrap did not produce a usable binary: {staged_binary}')
        if not binary_has_macos_arch(staged_binary, macos_arch):
            raise RuntimeError(
                f'bundled Ninja bootstrap produced the wrong macOS architecture: '
                f'expected {macos_arch or "native"}'
            )
        published_binary = staging / staged_binary.name
        shutil.copy2(staged_binary, published_binary)
        if not binary_is_usable(published_binary):
            raise RuntimeError(f'copied bundled Ninja binary is unusable: {published_binary}')
        if not binary_has_macos_arch(published_binary, macos_arch):
            raise RuntimeError(
                f'copied bundled Ninja has the wrong macOS architecture: '
                f'expected {macos_arch or "native"}'
            )
        shutil.rmtree(staged_source)
        shutil.rmtree(bootstrap_dir)
        (staging / '.whp-ninja-tool').write_text(expected_marker, encoding='utf-8')
        atomic_install(staging, final_dir)
    except Exception:
        shutil.rmtree(staging, ignore_errors=True)
        raise

    binary = ninja_binary(final_dir)
    print(f'Bundled Ninja ready: {binary}', file=sys.stderr)
    return binary.resolve()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', required=True)
    args = parser.parse_args()

    try:
        path = ensure_bundled_ninja(pathlib.Path(args.build_dir).expanduser().resolve())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f'error: bundled Ninja bootstrap failed: {exc}', file=sys.stderr)
        return 1

    print(path)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
