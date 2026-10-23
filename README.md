## INTRODUCTION

Wine is a program which allows running Microsoft Windows programs
(including DOS, Windows 3.x, Win32, and Win64 executables) on Unix.
It consists of a program loader which loads and executes a Microsoft
Windows binary, and a library (called Winelib) that implements Windows
API calls using their Unix, X11 or Mac equivalents.  The library may also
be used for porting Windows code into native Unix executables.

Wine is free software, released under the GNU LGPL; see the file
LICENSE for the details.

## Windows application compatibility and provenance

Before treating a historical filename or Microsoft-branded product as a
Windows OS component, separate its functional owner (OS, optional runtime,
driver, application or application-private support library) from its
distribution history (original media, OEM bundle, independent release,
application update or OS hotfix). A file shipped on installation media
does not necessarily belong to a default OS installation or run there.

See [the Windows application compatibility ledger](documentation/windows-application-compatibility.md)
for the Microsoft Arcade (1993) and Microsoft Dangerous Creatures (1994)
test cases, their Win16, CD-ROM, GDI, multimedia and INI boundaries,
original-source links, and the evidence required before changing Water.
This ledger records requirements and proposed probes; it does not claim
that either original program has been executed successfully.


## Windows 95 RTM DDK compatibility reference headers

The `include/ddk/win95rtm/` headers are WHP-authored, version-scoped
**compile-time reference boundaries** shared with the separately pinned
`WHP-Drivers` project. `vmm.h` describes an i386 VxD DDB candidate;
`win16_abi.h` checks OpenWatcom 16-bit pointer sizes for audio/display;
`display/ddk_minidrv.h` requires an opt-in historical Win95 display DDK;
`display/minivdd.h` records selected 9x mini-VDD data for comparison;
and `audio/ddk_audio.h` opts into historical Win16 sound-driver headers.
These are not complete or certified Windows 95 RTM DDK headers.
They do **not** provide DIBENGINE/GDIINFO layouts, VDD dispatch calling
conventions, a driver runtime, or Windows 95 RTM import libraries. Do not
substitute NT/WDM headers or Water's Win32 libraries for those components.
The separate `network/ndis31.h` boundary can opt into a verified NT 3.51
NDIS DDK. Its API is not replaced by the generic `include/ddk/ndis.h`.
The driver-only `minivdd.h` legacy-name adapter and device-specific headers
remain in WHP-Drivers. The consumer must pin Water by gitlink revision.

## QUICK START

From the top-level directory of the Water source (which contains this file),
run:

```
./build.sh
```

The build funnel regenerates the ignored `./configure` script from
`configure.ac` with Autoconf, initializes the pinned submodules, selects the
LLVM toolchain, configures an out-of-tree build, and invokes Ninja or Make.
The pinned bootstrap graph is transitive: the Bash fork consumes
`AM_PROG_INSTALL_SH` and `AM_PROG_INSTALL_STRIP` from its vendored `aclocal.m4`,
so Water also pins the Automake fork that provides those macros. Water validates
that provenance during the Bash bootstrap; it does not require a separate
system-installed `automake` executable for the normal build.

Water records the source checksums, pinned Autoconf gitlink and generated
`configure` checksum under `build/.whp-autoconf-inputs` after successful
generation. Subsequent builds with unchanged inputs reuse that verified
`configure` without starting the pinned Autoconf bootstrap. Changed inputs
or a changed gitlink invalidate the cache and require regeneration. A fresh
Git checkout still needs an Autoconf generator: the pinned fork's first
bootstrap needs GNU M4, Perl and host Automake tools. On systems where the
installed Autoconf can process Water's sources, use
`WATER_AUTOCONF_PROVIDER=host ./build.sh` to avoid that bootstrap.
The pinned Bash source already includes a generated `configure`; Water
preserves it instead of invoking an older host Autoconf to regenerate it.
Run `sh scripts/tests/configure-cache.sh` to verify the cache safeguards.

For an offline or older development host **after a pinned generation has
succeeded**, use `WATER_AUTOCONF_PROVIDER=cached` to require the verified
generated `configure` without invoking any generator:

```sh
WHP_GIT_UPDATE=0 WHP_SUBMODULES=0 WATER_AUTOCONF_PROVIDER=cached ./build.sh incremental
```

The `cached` option is available in `./build.sh menuconfig`. It checks the
pinned gitlink and input/output hashes; a missing or invalid cache **fails**
rather than falling back to a moving Autoconf, guessing a version, or
regenerating stale content. The `WHP_SUBMODULES=0` example assumes all
other required build dependencies are already present. Selecting
`AUTOCONF` explicitly conflicts with cached-only mode, because a generator
override cannot establish pinned provenance.

**Version boundaries:** Water's `configure.ac` requires Autoconf `2.63b`
at script-generation time. The pinned **Autoconf source fork itself** has
`AC_PREREQ([2.69])`; that is a bootstrap-source requirement, not the
minimum Autoconf version for running an already generated `configure`.
For an explicit system generator instead of bootstrapping the fork, select
`WATER_AUTOCONF_PROVIDER=host` (or supply `AUTOCONF=/path/to/autoconf`).
A host generator still has to support the actual macros in Water's
`configure.ac`; an arbitrarily old version is not assumed compatible.
Neither route weakens the fork's minimum version or silently imports newer
Autotools macros into Windows 95 driver headers.


The LLVM libc++ PE runtime cache at `build/llvm-libcxx-pe/` is audited
before reuse. Each archive member must have the expected COFF machine type
for its target (including AArch64 and ARM64EC), so a stale x64 or mixed
archive is rebuilt instead of entering Water's link. The legacy bundled
libraries remain available for architectures that still require them.
Run `sh scripts/tests/libcxx-archive-machine.sh` with Clang and llvm-ar
installed to exercise the archive check independently.

Normal builds now check Water's configured tracking branch before any build
bootstrap. If a newer Water revision can fast-forward safely, `./build.sh`
updates it and immediately restarts the refreshed build script in the same
invocation. On an unchanged upstream, the updater avoids fetching objects.
The recorded dependency submodule pins remain authoritative:

```
./build.sh update                              # Fast-forward Water, then initialize pinned submodules
./build.sh update-deps toolchains/llvm-project # Fetch the LLVM fork's configured branch tip
```

`update-deps` requires a clean checkout, accepts explicitly named submodules,
and stages updated gitlink revisions for inspection. It does **not** create
commits or push them. Validate the updated toolchain before committing and
pushing the new pins; otherwise the Water branch will continue to use its
recorded revisions. `update` forces a strict source refresh and initializes the pinned
submodules. Source updates use the configured branch/upstream, fetch only when
its commit changes, and merge using `--ff-only` (never rebase or force-push).
`update-deps` is still the only operation that stages newer submodule pins.
`WHP_GIT_UPDATE=auto` is the normal default: CI, source archives, detached
checkouts, uncommitted tracked changes, and temporarily unreachable remotes
retain the current source rather than silently discarding work or blocking an
offline build when its pinned dependencies are already present.
When Water is checked out as a submodule (such as QEMU's `toolchains/wine`),
automatic updates verify its detached HEAD against the consuming repository's
index gitlink. A matching pin is reported as expected, without changing
revisions; a mismatch still warns, and strict updates still reject a detached
checkout rather than silently attaching it to a branch.
Set `WHP_GIT_UPDATE=1` for strict failure on those conditions, or
`WHP_GIT_UPDATE=0` to disable source checks. To disable dependency
initialization as well for a fully offline build with existing toolchains,
use `WHP_GIT_UPDATE=0 WHP_SUBMODULES=0 ./build.sh`. `incremental`,
`clean`, `distclean`, `menuconfig`, and `update-deps` skip the network
check by default; an explicit `WHP_GIT_UPDATE` can override the default.
An advanced Water revision always re-enters the new `build.sh` at most once.
Use `./build.sh menuconfig` to edit the persistent ignored `.whpconfig`
profile. The menu controls PE architectures, the WHP LLVM bootstrap and host
linker policies, build behavior, installation, and optional Wine components. The LLVM menu
defaults to a lean Water-only graph: unused tests, docs, examples, benchmarks,
utilities, runtimes, bindings, and default tool sweeps are excluded while
required Clang, LLD, archive, symbol, and strip tools remain available. The
automatic linker mode uses host LLD only where it is part of the normal
bootstrap. On macOS, automatic mode keeps the LLVM bootstrap independent of
the optional Mach-O LLD backend and uses Apple ld; explicitly selecting LLD
opts into Mach-O and requires an Objective-C/Foundation link probe to pass.
arm64e macOS remains on Apple ld.
It uses the same curses UI as the QEMU funnel when Python is available and
falls back to a POSIX-shell menu otherwise.

Use `./build.sh configure [options]` when one-run or advanced configure
options are needed; explicit command-line options override the saved profile.

To make the normal build command install Water automatically, set **Install after
successful build** in `./build.sh menuconfig` to `runtime`, `development`, or
`all`. Set **Install prefix** in the same menu to the desired configure prefix
(default `/usr/local`). A normal `./build.sh` or `./build.sh build` then performs
the selected install only after the full build succeeds. Explicit internal build
targets do not trigger the automatic install.

Changing the prefix causes Water to reconfigure because install paths are part of
the configured build. Changing only the post-build install mode does not.

Or install the configured Water runtime explicitly (recommended for a manual
install):

```
./build.sh install-lib
```

`install-lib` leaves development-only headers, import libraries, static libraries,
and the bundled legacy libc++/libc++abi development archives out of a normal
runtime installation. If a development environment is intentionally needed,
use `./build.sh install-dev`; use plain `./build.sh install` only when both the
runtime and development files are wanted. With the default `WATER_LIBCXX=llvm`,
`install-dev` installs the generated LLVM-fork libc++ headers under a per-architecture
`wine/c++/<arch>/v1` tree and installs `libwhp-libcxx.a` beside that architecture's
Water PE libraries. The bundled libc++ 8 headers and `libc++.a`/`libc++abi.a` are
kept only when a selected ABI still requires the legacy provider (currently ARM or
PowerPC) or when an explicit external C++ provider needs the compatibility fallback.

Before configuring, use `./build.sh menuconfig` or explicit configure options to
disable optional components you do not want in the configured build. The install
targets operate on that configured component graph rather than adding disabled
features back at install time.

Or run Water directly from the build directory:

```
./build/wine notepad
```

Run programs as `wine program`. For more information and problem
resolution, read the rest of this file, the Wine man page, and
especially the wealth of information found at https://www.winehq.org.


## REQUIREMENTS

To compile and run Wine, you must have one of the following:

- Linux version 2.6.22 or later
- FreeBSD 12.4 or later
- Solaris x86 9 or later
- NetBSD-current
- macOS 10.15 or later

As Wine requires kernel-level thread support to run, only the operating
systems mentioned above are supported.  Other operating systems which
support kernel threads may be supported in the future.

**FreeBSD info**:
  See https://wiki.freebsd.org/Wine for more information.

**Solaris info**:
  You will most likely need to build Wine with the GNU toolchain
  (gcc, gas, etc.). Warning : installing gas does *not* ensure that it
  will be used by gcc. Recompiling gcc after installing gas or
  symlinking cc, as and ld to the gnu tools is said to be necessary.

**NetBSD info**:
  Make sure you have the USER_LDT, SYSVSHM, SYSVSEM, and SYSVMSG options
  turned on in your kernel.

**macOS info**:
  You need Xcode/Xcode Command Line Tools or Apple cctools.

**Supported file systems**:
  Wine should run on most file systems. A few compatibility problems
  have also been reported using files accessed through Samba. Also,
  NTFS does not provide all the file system features needed by some
  applications.  Using a native Unix file system is recommended.

**Basic requirements**:
  You need to have the X11 development include files installed
  (called xorg-dev in Debian and libX11-devel in Red Hat).
  Of course you also need make (most likely GNU make).
  You also need flex version 2.5.33 or later and bison.

**Optional support libraries**:
  Configure will display notices when optional libraries are not found
  on your system. See https://gitlab.winehq.org/wine/wine/-/wikis/Building-Wine
  for hints about the packages you should install. On 64-bit
  platforms, you have to make sure to install the 32-bit versions of
  these libraries.


## COMPILATION

To build Wine, do:

```
./configure
make
```

This will build the program "wine" and numerous support libraries/binaries.
The program "wine" will load and run Windows executables.
The library "libwine" ("Winelib") can be used to compile and link
Windows source code under Unix.

To see compile configuration options, do `./configure --help`.

For more information, see https://gitlab.winehq.org/wine/wine/-/wikis/Building-Wine


## SETUP

Once Water has been built correctly, either enable post-build installation in
`./build.sh menuconfig` or run `./build.sh install-lib` explicitly for a
runtime installation. This installs the runtime side of the configured build
without also installing development headers, import libraries, or static
libraries. Use `./build.sh install-dev` only when those development files are
needed, and use `./build.sh install` only when both sets are intentionally
wanted. The default LLVM libc++ provider is installed from the pinned LLVM fork rather
than from Water's old bundled libc++ tree on supported PE ABIs. Disable unwanted optional components in `./build.sh menuconfig` (or
with configure options) before building so they are absent from the install
graph as well.

Don't forget to uninstall any conflicting previous Wine installation
first.  Try either `dpkg -r wine` or `rpm -e wine` or `make uninstall`
before installing.

Once installed, you can run the `winecfg` configuration tool. See the
Support area at https://www.winehq.org/ for configuration hints.


## RUNNING PROGRAMS

When invoking Wine, you may specify the entire path to the executable,
or a filename only.

For example, to run Notepad:

```
wine notepad            (using the search Path as specified in
wine notepad.exe         the registry to locate the file)

wine c:\\windows\\notepad.exe      (using DOS filename syntax)

wine ~/.wine/drive_c/windows/notepad.exe  (using Unix filename syntax)

wine notepad.exe readme.txt          (calling program with parameters)
```

Wine is not perfect, so some programs may crash. If that happens you
will get a crash log that you should attach to your report when filing
a bug.


## GETTING MORE INFORMATION

- **WWW**: A great deal of information about Wine is available from WineHQ at
	https://www.winehq.org/ : various Wine Guides, application database,
	bug tracking. This is probably the best starting point.

- **FAQ**: The Wine FAQ is located at https://gitlab.winehq.org/wine/wine/-/wikis/FAQ

- **Wiki**: The Wine Wiki is located at https://gitlab.winehq.org/wine/wine/-/wikis/

- **Gitlab**: Wine development is hosted at https://gitlab.winehq.org

- **Mailing lists**:
	There are several mailing lists for Wine users and developers; see
	https://gitlab.winehq.org/wine/wine/-/wikis/Forums for more
	information.

- **Bugs**: Report bugs to Wine Bugzilla at https://bugs.winehq.org
	Please search the bugzilla database to check whether your
	problem is already known or fixed before posting a bug report.

- **IRC**: Online help is available at channel `#WineHQ` on irc.libera.chat.
