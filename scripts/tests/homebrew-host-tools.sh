#!/bin/sh
# Homebrew is optional: locate installed formula tools without changing PATH.
set -eu
root=$(CDPATH= cd -P -- "$(dirname -- "$0")/../.." && pwd -P)
helper=$root/scripts/whp-build/find-host-tool.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-homebrew-tools.XXXXXXXX")
trap 'rm -rf -- "$tmp"' 0
trap 'exit 1' 1 2 3 15
/bin/sh -n "$helper"

mkdir -p "$tmp/bin" "$tmp/brew prefix/opt/texinfo/bin" "$tmp/custom/texinfo/bin"
cat > "$tmp/bin/uname" <<'EOF'
#!/bin/sh
case "$1" in -s) printf 'Darwin\n' ;; -m) printf 'arm64\n' ;; esac
EOF
cat > "$tmp/brew prefix/opt/texinfo/bin/makeinfo" <<'EOF'
#!/bin/sh
printf 'makeinfo (GNU texinfo) mock\n'
EOF
cat > "$tmp/custom/texinfo/bin/makeinfo" <<'EOF'
#!/bin/sh
printf 'makeinfo (GNU texinfo) custom\n'
EOF
chmod +x "$tmp/bin/uname" "$tmp/brew prefix/opt/texinfo/bin/makeinfo" "$tmp/custom/texinfo/bin/makeinfo"

# Find a keg-only executable with brew entirely absent from PATH.
found=$(PATH="$tmp/bin:/usr/bin:/bin" HOMEBREW_PREFIX="$tmp/brew prefix" \
    /bin/sh "$helper" --brew-only makeinfo texinfo)
[ "$found" = "$tmp/brew prefix/opt/texinfo/bin/makeinfo" ] ||
    { echo 'FAIL: keg-only executable not found' >&2; exit 1; }

# A PATH executable wins by default, while --brew-only bypasses it.
cat > "$tmp/bin/makeinfo" <<'EOF'
#!/bin/sh
printf 'PATH makeinfo\n'
EOF
chmod +x "$tmp/bin/makeinfo"
found=$(PATH="$tmp/bin:/usr/bin:/bin" HOMEBREW_PREFIX="$tmp/brew prefix" \
    /bin/sh "$helper" makeinfo texinfo)
[ "$found" = "$tmp/bin/makeinfo" ] || {
    echo 'FAIL: PATH selection was overridden' >&2; exit 1;
}

# A nonstandard Homebrew prefix can be located by brew --prefix --installed.
cat > "$tmp/bin/brew" <<'EOF'
#!/bin/sh
[ "$#" -eq 3 ] && [ "$1" = --prefix ] && [ "$2" = --installed ] &&
[ "$3" = texinfo ] || exit 1
printf '%s\n' "$TEST_BREW_CUSTOM"
EOF
chmod +x "$tmp/bin/brew"
found=$(PATH="$tmp/bin:/usr/bin:/bin" HOMEBREW_PREFIX= \
    TEST_BREW_CUSTOM="$tmp/custom/texinfo" \
    /bin/sh "$helper" --brew-only makeinfo texinfo)
[ "$found" = "$tmp/custom/texinfo/bin/makeinfo" ] || {
    echo 'FAIL: custom prefix unavailable through brew' >&2; exit 1;
}

# No phantom dependency, no injection via formula, and no Linux brew fallback.
if PATH="$tmp/bin:/usr/bin:/bin" HOMEBREW_PREFIX= \
    TEST_BREW_CUSTOM= /bin/sh "$helper" --brew-only missingtool missingformula >/dev/null; then
    echo 'FAIL: nonexistent formula tool was invented' >&2; exit 1
fi
if /bin/sh "$helper" makeinfo '../unsafe' >/dev/null 2>&1; then
    echo 'FAIL: unsafe formula name accepted' >&2; exit 1
fi
cat > "$tmp/bin/uname" <<'EOF'
#!/bin/sh
case "$1" in -s) printf 'Linux\n' ;; -m) printf 'x86_64\n' ;; esac
EOF
chmod +x "$tmp/bin/uname"
if PATH="$tmp/bin:/usr/bin:/bin" HOMEBREW_PREFIX="$tmp/brew prefix" \
    /bin/sh "$helper" --brew-only makeinfo texinfo >/dev/null; then
    echo 'FAIL: Homebrew fallback leaked onto non-macOS' >&2; exit 1
fi
echo 'PASS: PATH priority, keg-only Homebrew, custom prefix, safe no-brew fallback, Linux isolation'
