#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-2.1-or-later

from __future__ import annotations

import argparse
import os
import pathlib
import sys
from collections import OrderedDict
from typing import Dict, List, NamedTuple, Optional, Tuple

CONFIG_VERSION = '1'


class Option(NamedTuple):
    key: str
    section: str
    label: str
    kind: str
    default: str
    choices: Tuple[str, ...] = ()
    group: str = ''


OPTIONAL_COMPONENTS = (
    ('ALSA', 'ALSA audio'),
    ('CAPI', 'CAPI / ISDN'),
    ('COREAUDIO', 'CoreAudio'),
    ('CUPS', 'CUPS printing'),
    ('DBUS', 'D-Bus'),
    ('FFMPEG', 'FFmpeg'),
    ('FONTCONFIG', 'Fontconfig'),
    ('FREETYPE', 'FreeType'),
    ('GETTEXT', 'Gettext'),
    ('GETTEXTPO', 'GettextPO'),
    ('GPHOTO', 'gPhoto'),
    ('GNUTLS', 'GnuTLS'),
    ('GSSAPI', 'GSSAPI / Kerberos'),
    ('GSTREAMER', 'GStreamer'),
    ('HWLOC', 'hwloc'),
    ('INOTIFY', 'inotify'),
    ('KRB5', 'Kerberos 5'),
    ('MINGW', 'MinGW / PE cross compiler'),
    ('NETAPI', 'Samba NetAPI'),
    ('OPENCL', 'OpenCL'),
    ('OPENGL', 'OpenGL'),
    ('OSS', 'OSS audio'),
    ('PCAP', 'Packet capture'),
    ('PCSCLITE', 'PC/SC Lite'),
    ('PTHREAD', 'pthread'),
    ('PULSE', 'PulseAudio'),
    ('SANE', 'SANE scanners'),
    ('SDL', 'SDL'),
    ('UDEV', 'udev'),
    ('USB', 'libusb'),
    ('V4L2', 'Video4Linux2'),
    ('VA', 'VA-API'),
    ('VULKAN', 'Vulkan'),
    ('WAYLAND', 'Wayland'),
    ('XCOMPOSITE', 'XComposite'),
    ('XCURSOR', 'Xcursor'),
    ('XFIXES', 'Xfixes'),
    ('XINERAMA', 'Xinerama'),
    ('XINPUT', 'XInput'),
    ('XINPUT2', 'XInput2'),
    ('XRANDR', 'XRandR'),
    ('XRENDER', 'XRender'),
    ('XSHAPE', 'XShape'),
    ('XSHM', 'XShm'),
    ('XXF86VM', 'XFree86 VidMode'),
)


OPTIONS = (
    Option('WATER_ARCHS_MODE', 'Architectures', 'PE architecture selection', 'choice',
           'auto', ('auto', 'custom', 'none')),
    Option('WATER_ARCH_I386', 'Architectures', 'i386', 'bool', 'y'),
    Option('WATER_ARCH_X86_64', 'Architectures', 'x86_64', 'bool', 'y'),
    Option('WATER_ARCH_ARM', 'Architectures', 'ARM', 'bool', 'y'),
    Option('WATER_ARCH_AARCH64', 'Architectures', 'AArch64', 'bool', 'y'),
    Option('WATER_ARCH_ARM64EC', 'Architectures', 'ARM64EC', 'bool', 'y'),
    Option('WATER_ARCH_POWERPC', 'Architectures', 'PowerPC', 'bool', 'y'),

    Option('WATER_LLVM_BOOTSTRAP', 'LLVM toolchain', 'Bootstrap/use WHP LLVM',
           'choice', 'auto', ('auto', 'y', 'n')),
    Option('WATER_LLVM_BUILD_TYPE', 'LLVM toolchain', 'LLVM build type', 'choice',
           'Release', ('Release', 'RelWithDebInfo', 'Debug')),
    Option('WATER_LLVM_ASSERTIONS', 'LLVM toolchain', 'LLVM assertions', 'bool', 'n'),

    Option('WATER_WIN16', 'Build behavior', 'Win16 support', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_WIN64', 'Build behavior', 'Win64-only build', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_TESTS', 'Build behavior', 'Regression tests', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_BUILD_ID', 'Build behavior', 'Build ID sections', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_NINJA', 'Build behavior', 'Generate/prefer Ninja files', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_MAINTAINER_MODE', 'Build behavior', 'Maintainer mode', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_SAST', 'Build behavior', 'Clang static analysis / SAST', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_SILENT_RULES', 'Build behavior', 'Silent build rules', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_WERROR', 'Build behavior', 'Treat warnings as errors', 'choice',
           'auto', ('auto', 'y', 'n')),
    Option('WATER_COMPILER_CACHE', 'Build behavior', 'Compiler cache', 'choice',
           'auto', ('auto', 'sccache', 'ccache', 'none')),

    *tuple(
        Option(f'WATER_WITH_{key}', 'Optional components', label, 'choice',
               'auto', ('auto', 'y', 'n'))
        for key, label in OPTIONAL_COMPONENTS
    ),

    Option('WATER_SYSTEM_DLLPATH', 'Advanced paths', 'System DLL search path',
           'string', 'auto'),
    Option('WATER_WINE_TOOLS', 'Advanced paths', 'External Wine tools directory',
           'string', 'auto'),
    Option('WATER_WINE64', 'Advanced paths', 'External 64-bit Wine build directory',
           'string', 'auto'),
)

OPTION_BY_KEY = {option.key: option for option in OPTIONS}


class ConfigState:
    def __init__(self, values: Dict[str, str], unknown: Optional[Dict[str, str]] = None):
        self.values = values
        self.unknown = OrderedDict(unknown or {})


def default_values() -> Dict[str, str]:
    return {option.key: option.default for option in OPTIONS}


def validate_value(option: Option, value: str) -> None:
    if option.kind == 'bool':
        if value not in ('y', 'n'):
            raise ValueError(f'{option.key} must be y or n')
        return
    if option.kind == 'choice':
        if value not in option.choices:
            raise ValueError(
                f"{option.key} must be one of: {', '.join(option.choices)}"
            )
        return
    if option.kind == 'string':
        if not value or any(ch in value for ch in ('\n', '\r', '\0')):
            raise ValueError(f'{option.key} contains an invalid line break or NUL')
        return
    raise ValueError(f'unsupported option type for {option.key}')


def load_config(path: pathlib.Path) -> ConfigState:
    values = default_values()
    unknown: OrderedDict[str, str] = OrderedDict()
    if not path.exists():
        return ConfigState(values, unknown)

    version = None
    for number, raw_line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith('#'):
            continue
        if '=' not in line:
            raise ValueError(f'{path}:{number}: expected KEY=VALUE')
        key, value = line.split('=', 1)
        key = key.strip()
        value = value.strip()
        if key == 'WHP_CONFIG_VERSION':
            version = value
            if value != CONFIG_VERSION:
                raise ValueError(
                    f'{path}:{number}: unsupported WHP_CONFIG_VERSION={value}'
                )
            continue
        option = OPTION_BY_KEY.get(key)
        if option is None:
            unknown[key] = value
            continue
        validate_value(option, value)
        values[key] = value

    if path.stat().st_size and version is None:
        print(
            f'warning: {path} has no WHP_CONFIG_VERSION; treating it as version 1',
            file=sys.stderr,
        )
    for key in unknown:
        print(f'warning: saved option {key} is no longer recognized', file=sys.stderr)
    return ConfigState(values, unknown)


def render_config(state: ConfigState) -> str:
    lines = [
        '# WHP Water portable user configuration',
        '# This file is user-owned and ignored by Git.',
        f'WHP_CONFIG_VERSION={CONFIG_VERSION}',
        '',
    ]
    previous_section = None
    for option in OPTIONS:
        if option.section != previous_section:
            if previous_section is not None:
                lines.append('')
            lines.append(f'# {option.section}')
            previous_section = option.section
        value = state.values[option.key]
        validate_value(option, value)
        lines.append(f'{option.key}={value}')
    if state.unknown:
        lines.extend(['', '# Preserved settings not recognized by this checkout'])
        for key, value in state.unknown.items():
            lines.append(f'{key}={value}')
    return '\n'.join(lines) + '\n'


def save_config(path: pathlib.Path, state: ConfigState) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    content = render_config(state)
    if path.exists() and path.read_text(encoding='utf-8') == content:
        return
    temp = path.with_name(path.name + '.tmp')
    temp.write_text(content, encoding='utf-8')
    os.replace(temp, path)


def shell_assignments(state: ConfigState, environ: Dict[str, str]) -> str:
    lines: List[str] = []
    for option in OPTIONS:
        key = option.key
        if key in environ:
            continue
        value = state.values[key]
        if value == 'auto':
            continue
        quoted = "'" + value.replace("'", "'\"'\"'") + "'"
        lines.append(f'{key}={quoted}')
        lines.append(f'export {key}')
    return '\n'.join(lines) + ('\n' if lines else '')


def sections() -> List[Tuple[str, List[Option]]]:
    result: List[Tuple[str, List[Option]]] = []
    for option in OPTIONS:
        if not result or result[-1][0] != option.section:
            result.append((option.section, []))
        result[-1][1].append(option)
    return result


def main(argv: Optional[List[str]] = None) -> int:
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--shell', metavar='CONFIG')
    group.add_argument('--dump-menu', action='store_true')
    args = parser.parse_args(argv)

    try:
        if args.shell:
            state = load_config(pathlib.Path(args.shell))
            sys.stdout.write(shell_assignments(state, dict(os.environ)))
            return 0
        if args.dump_menu:
            for section, options in sections():
                print(section)
                for option in options:
                    print(f'  {option.key}={option.default}')
            return 0
    except (OSError, ValueError) as exc:
        print(f'error: {exc}', file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
