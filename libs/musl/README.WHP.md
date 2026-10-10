# musl source ownership for Water

Water contains a deliberately selected **musl 1.2.3** math/complex
implementation at `libs/musl/src`. Its `Makefile.in` builds these
sources into `libmusl.a` using Water's own compatibility headers.
This is **not** a Linux/musl C runtime used in place of Water's Windows
CRT or a Windows DLL.

## Linked fork

The optional, revision-pinned Git submodule at `libs/musl/upstream`
references `https://github.com/retrochristian5000/musl-libc.git`.
The linked fork currently declares **musl 1.2.6**. This source tree
does **not** replace the older selected implementation automatically.

To inspect the pinned source explicitly:

```sh
git submodule update --init libs/musl/upstream
```

To intentionally advance its pin using Water's existing update workflow:

```sh
./build.sh update-deps libs/musl/upstream
```

Review and commit the staged gitlink change before other builds depend
on a new upstream version. Ordinary `./build.sh build` uses the
checked-in `libs/musl/src` compatibility subset; it does **not**
download the full musl fork.

## Porting gate

Do not substitute the upstream `src` tree for Water's `src` tree by
renaming paths or adding the upstream include directory wholesale.
Water's subset has a Windows-oriented `src/internal/features.h` and
modified math/internal sources. The current fork is **not** a
drop-in source-identical replacement. Before switching the Makefile to
the pinned upstream source, compare all selected C and header files,
reconcile the Windows adaptations, and test Win32/Win64 build, exports,
math edge cases, and architecture-specific ABI behavior. Keep the
Windows CRT and musl/Linux libc responsibilities separate.

The musl fork is pinned for provenance and controlled migration, not
as an unverified mandatory build-time clone.
