#!/bin/sh
# Standalone POSIX regression for the GNU M4 selection adapter.
set -eu
root=$(CDPATH= cd -P "$(dirname "$0")/.." && pwd)
adapter=$root/whp-build/m4.sh
sandbox=$(mktemp -d "${TMPDIR:-/tmp}/water-m4-test.XXXXXX")
trap 'rm -rf "$sandbox"' 0
trap 'exit 1' 1 2 3 15
mkdir -p "$sandbox/bin" "$sandbox/explicit with spaces"
cat > "$sandbox/bin/gm4" <<'EOF'
#!/bin/sh
if [ "${1:-}" = --gnu ] && [ "${2:-}" = --version ]; then
    echo 'gm4 (GNU M4) 1.4.20'; exit 0
fi
if [ "$#" -eq 1 ] && [ "$1" = --gnu ]; then
    cat >/dev/null
    printf 'whp-water-m4-ok\n'; exit 0
fi
printf 'GNU command:'
printf ' <%s>' "$@"
printf '\n'
EOF
cat > "$sandbox/bin/m4" <<'EOF'
#!/bin/sh
if [ "${1:-}" = --gnu ] && [ "${2:-}" = --version ]; then
    echo 'm4 (BSD m4) 1.0'; exit 0
fi
printf 'WRONG m4\n'
EOF
cat > "$sandbox/explicit with spaces/m4-real" <<'EOF'
#!/bin/sh
if [ "${1:-}" = --gnu ] && [ "${2:-}" = --version ]; then
    echo 'm4 (GNU M4) 1.4.20'; exit 0
fi
if [ "$#" -eq 1 ] && [ "$1" = --gnu ]; then
    cat >/dev/null
    printf 'whp-water-m4-ok\n'; exit 0
fi
printf 'EXPLICIT command:'
printf ' <%s>' "$@"
printf '\n'
EOF
chmod +x "$sandbox/bin/"* "$sandbox/explicit with spaces/m4-real"
# gm4 must win over an incompatible BSD m4.
actual=$(PATH="$sandbox/bin:$PATH" M4= /bin/sh "$adapter" --print-path)
[ "$actual" = "$sandbox/bin/gm4" ] || { echo 'gm4 not preferred' >&2; exit 1; }
[ "$(PATH="$sandbox/bin:$PATH" M4= /bin/sh "$adapter" --check)" = "WHP GNU M4: $sandbox/bin/gm4" ] ||
    { echo 'M4 --check did not report the verified binary' >&2; exit 1; }
actual=$(PATH="$sandbox/bin:$PATH" M4= /bin/sh "$adapter" 'test()' 'two words')
[ "$actual" = 'GNU command: <test()> <two words>' ] || { echo 'arguments not preserved' >&2; exit 1; }
# An explicit executable must override PATH selection, including spaces.
actual=$(PATH="$sandbox/bin:$PATH" M4="$sandbox/explicit with spaces/m4-real" /bin/sh "$adapter" --print-path)
[ "$actual" = "$sandbox/explicit with spaces/m4-real" ] || { echo 'explicit path failed' >&2; exit 1; }
actual=$(PATH="$sandbox/bin:$PATH" M4="$sandbox/explicit with spaces/m4-real" /bin/sh "$adapter" abc)
[ "$actual" = 'EXPLICIT command: <abc>' ] || { echo 'explicit execution failed' >&2; exit 1; }
# Invalid overrides fail instead of switching to the PATH-selected gm4.
if PATH="$sandbox/bin:$PATH" M4="$sandbox/bin/m4" /bin/sh "$adapter" --print-path > "$sandbox/invalid.log" 2>&1; then
    echo 'BSD m4 override wrongly accepted' >&2; exit 1
fi
if PATH="$sandbox/bin:$PATH" M4=/missing/water-m4 /bin/sh "$adapter" --print-path > "$sandbox/missing.log" 2>&1; then
    echo 'missing M4 override wrongly accepted' >&2; exit 1
fi
if PATH="$sandbox/bin:$PATH" M4="$adapter" /bin/sh "$adapter" --print-path > "$sandbox/recursion.log" 2>&1; then
    echo 'recursive M4 override wrongly accepted' >&2; exit 1
fi
# Symlinks back into the adapter must not hide recursion.
ln -s "$adapter" "$sandbox/bin/recursive-link"
ln -s recursive-link "$sandbox/bin/recursive-m4"
if PATH="$sandbox/bin:$PATH" M4="$sandbox/bin/recursive-m4" /bin/sh "$adapter" --print-path > "$sandbox/recursion.log" 2>&1; then
    echo 'symlinked recursive M4 override wrongly accepted' >&2; exit 1
fi
# A --version shim is not sufficient; macro expansion must actually work.
cat > "$sandbox/version-only" <<'EOF'
#!/bin/sh
if [ "${1:-}" = --gnu ] && [ "${2:-}" = --version ]; then
    printf 'GNU M4 1.4.20\n'
    exit 0
fi
exit 0
EOF
chmod +x "$sandbox/version-only"
if PATH="$sandbox/bin:$PATH" M4="$sandbox/version-only" /bin/sh "$adapter" --print-path > "$sandbox/version-only.log" 2>&1; then
    echo 'version-only M4 shim wrongly accepted' >&2; exit 1
fi
# If gm4 is invalid, the fallback m4 is still checked and accepted if GNU.
cp "$sandbox/explicit with spaces/m4-real" "$sandbox/bin/m4"
cp "$sandbox/bin/gm4" "$sandbox/gm4.saved"
cp "$sandbox/bin/m4" "$sandbox/bin/gm4"
cat > "$sandbox/bin/gm4" <<'EOF'
#!/bin/sh
printf 'not GNU M4\n'
exit 1
EOF
chmod +x "$sandbox/bin/gm4"
actual=$(PATH="$sandbox/bin:$PATH" M4= /bin/sh "$adapter" --print-path)
[ "$actual" = "$sandbox/bin/m4" ] || { echo 'm4 fallback failed' >&2; exit 1; }
printf 'PASS: M4 functional probe, overrides, symlink guard and argument forwarding\n'
