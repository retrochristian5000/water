#!/bin/sh
# Recovery regression using Water's real Autoconf helper and mocked host tools.
# No GNU M4, network, or Autoconf build is required.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-autoconf-cache.XXXXXXXX")
trap 'rm -rf "$tmp"' 0
trap 'exit 1' 1 2 3 15
command -v git >/dev/null 2>&1 || { echo 'SKIP: git unavailable'; exit 0; }
export GIT_ALLOW_PROTOCOL=file GIT_TERMINAL_PROMPT=0
mkdir -p "$tmp/tools" "$tmp/upstream" "$tmp/water/scripts/whp-build"
cp "$root/scripts/whp-build/ensure-autoconf.sh" "$tmp/water/scripts/whp-build/"
cp "$root/scripts/whp-build/m4.sh" "$tmp/water/scripts/whp-build/"

cat > "$tmp/upstream/bootstrap" <<'EOF'
#!/bin/sh
exit 0
EOF
cat > "$tmp/upstream/configure" <<'EOF'
#!/bin/sh
for option in "$@"; do
    case "$option" in --prefix=*) printf '%s\n' "${option#--prefix=}" > .mock-prefix ;; esac
done
test -s .mock-prefix
EOF
cat > "$tmp/tools/m4" <<'EOF'
#!/bin/sh
case "$1" in
    --gnu) [ "${2:-}" = --version ] && echo 'm4 (GNU M4) 1.4.20' ;;
    --version) echo 'm4 (GNU M4) 1.4.20' ;;
    *) exit 1 ;;
esac
EOF
cat > "$tmp/tools/perl" <<'EOF'
#!/bin/sh
printf 'Mock Perl\nPerl 5\n'
EOF
cat > "$tmp/tools/aclocal" <<'EOF'
#!/bin/sh
printf 'aclocal (GNU automake) mock\n'
EOF
cat > "$tmp/tools/automake" <<'EOF'
#!/bin/sh
printf 'automake (GNU automake) mock\n'
EOF
cat > "$tmp/tools/make" <<'EOF'
#!/bin/sh
case "${1:-}" in
    --version) echo 'GNU Make mock' ;;
    install)
        prefix=$(cat .mock-prefix)
        mkdir -p "$prefix/bin"
        cat > "$prefix/bin/autoconf" <<'EOC'
#!/bin/sh
case "${1:-}" in
    --version) echo 'autoconf (GNU Autoconf) mock' ;;
    -o) printf '#!/bin/sh\nexit 0\n' > "$2" ;;
    *) exit 1 ;;
esac
EOC
        chmod +x "$prefix/bin/autoconf"
        printf 'install\n' >> "$WHP_CACHE_INSTALL_LOG"
        ;;
esac
EOF
chmod +x "$tmp/upstream/bootstrap" "$tmp/upstream/configure" "$tmp/tools/"*
git init -q -b master "$tmp/upstream"
git -C "$tmp/upstream" config user.name 'Water test'
git -C "$tmp/upstream" config user.email 'test@example.invalid'
git -C "$tmp/upstream" add -A
git -C "$tmp/upstream" commit -qm 'mock Autoconf fork'

git init -q -b master "$tmp/water"
git -C "$tmp/water" config user.name 'Water test'
git -C "$tmp/water" config user.email 'test@example.invalid'
git -C "$tmp/water" submodule add -q "$tmp/upstream" toolchains/autoconf
git -C "$tmp/water" add -A
git -C "$tmp/water" commit -qm 'mock Water pin'

export WHP_CACHE_INSTALL_LOG=$tmp/installs
M4=$tmp/tools/m4
PERL=$tmp/tools/perl
ACLOCAL=$tmp/tools/aclocal
AUTOMAKE=$tmp/tools/automake
MAKE=$tmp/tools/make
export M4 PERL ACLOCAL AUTOMAKE MAKE
helper=$tmp/water/scripts/whp-build/ensure-autoconf.sh
cmd=$(/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0)
[ -x "$cmd" ] && [ "$(wc -l < "$WHP_CACHE_INSTALL_LOG" | tr -d ' ')" -eq 1 ]
prefix=$(dirname "$(dirname "$cmd")")
[ -f "$prefix/.whp-ready" ] || { echo 'FAIL: ready cache missing' >&2; exit 1; }

# Mismatched stage-0 Automake tools must not reuse a cached Autoconf binary.
cp "$tmp/tools/automake" "$tmp/automake.saved"
cat > "$tmp/tools/automake" <<'EOF'
#!/bin/sh
echo 'automake (GNU automake) different-version'
EOF
chmod +x "$tmp/tools/automake"
if /bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out" 2> "$tmp/err"; then
    echo 'FAIL: mismatched stage-0 Automake tools accepted' >&2; exit 1
fi
grep -q 'aclocal and automake versions differ' "$tmp/err" ||
    { echo 'FAIL: missing paired-stage-0 diagnostic' >&2; exit 1; }
mv "$tmp/automake.saved" "$tmp/tools/automake"

# A half-installed cache no longer strands the next build.
rm "$prefix/.whp-ready"
printf 'stale\n' > "$prefix/old-file"
[ "$(/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0)" = "$cmd" ]
[ ! -e "$prefix/old-file" ] &&
[ "$(wc -l < "$WHP_CACHE_INSTALL_LOG" | tr -d ' ')" -eq 2 ] ||
    { echo 'FAIL: partial cache not repaired' >&2; exit 1; }

# A lock owned by another worker must still block cleanup.
rm "$prefix/.whp-ready"
printf 'active\n' > "$prefix/active-file"
mkdir "$prefix.lock"
if /bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out" 2> "$tmp/err"; then
    echo 'FAIL: bootstrap ignored active lock' >&2; exit 1
fi
[ -f "$prefix/active-file" ] || { echo 'FAIL: locked cache was destroyed' >&2; exit 1; }
rmdir "$prefix.lock"
/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 >/dev/null
[ ! -f "$prefix/active-file" ] &&
[ "$(wc -l < "$WHP_CACHE_INSTALL_LOG" | tr -d ' ')" -eq 3 ] ||
    { echo 'FAIL: post-lock cache not repaired' >&2; exit 1; }

# A version-only impostor cannot satisfy the installed-runtime smoke test.
printf '#!/bin/sh\n[ "\${1:-}" = --version ] && echo "autoconf (GNU Autoconf) mock"\n' > "$cmd"
chmod +x "$cmd"
[ "$(/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0)" = "$cmd" ] ||
    { echo 'FAIL: version-only Autoconf cache was not repaired' >&2; exit 1; }
# A healthy rebuild must have occurred before the smoke-test validation.
[ "$(wc -l < "$WHP_CACHE_INSTALL_LOG" | tr -d ' ')" -eq 4 ] ||
    { echo 'FAIL: version-only cache did not trigger rebuild' >&2; exit 1; }

# A misleading ready marker with a broken executable must not be trusted.
printf '#!/bin/sh\nexit 1\n' > "$cmd"
chmod +x "$cmd"
/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 >/dev/null
[ "$(wc -l < "$WHP_CACHE_INSTALL_LOG" | tr -d ' ')" -eq 5 ] ||
    { echo 'FAIL: corrupt cache not repaired' >&2; exit 1; }
echo 'PASS: interrupted and corrupt Autoconf caches recover without bypassing lock'
