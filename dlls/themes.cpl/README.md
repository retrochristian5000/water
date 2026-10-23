# Classic Desktop Themes: initial Windows 98 Control Panel

Windows 98 themes are not Windows XP visual styles. Historically this feature
had both Themes.cpl and Themes.exe. This initial implementation adds the
Control Panel entry and file-selection path, not the full Themes.exe UI.

A selected classic .theme INI file can apply [Control Panel\\Colors] values
(RGB triplets) and [Control Panel\\Desktop] Wallpaper, TileWallpaper and
WallpaperStyle (classic 0 and 2 values). Paths with %ThemeDir% are resolved
relative to the selected theme file. No changes occur during installation:
the user must select a file and confirm applying it.

This does NOT yet cover theme previews, icons, cursors, sound schemes, screensavers,
font metrics, Active Desktop HTML/JPEG wallpaper conversion, themed assets,
monthly rotation, theme file association, or Windows 98 Plus! enhancements.
It does not use the XP uxtheme.dll or .msstyles format. It is an original
compatibility implementation rather than Microsoft's copyrighted binaries.

Host-run portable RGB parser test:

    cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
      dlls/themes.cpl/rgb.c dlls/themes.cpl/tests/rgb_test.c -o rgb-test
    ./rgb-test

Windows guest tests still required: load with control.exe themes.cpl, select a
local test .theme and BMP, inspect colors and wallpaper, and check persistence.
Malformed files should be rejected before making changes. System API failures
may still leave partial changes. Test Win98 separately from NT/XP personalities.
