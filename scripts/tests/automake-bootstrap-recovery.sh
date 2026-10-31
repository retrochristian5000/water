#!/bin/sh
# Exercise the pinned Automake stage without network or real Autotools.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-automake-bootstrap.XXXXXXXX")
trap 'rm -rf "$tmp"' 0
trap 'exit 1' 1 2 3 15
command -v git >/dev/null 2>&1 || { echo 'SKIP: git unavailable'; exit 0; }
export GIT_ALLOW_PROTOCOL=file GIT_TERMINAL_PROMPT=0
mkdir -p "$tmp/upstream" "$tmp/water/scripts/whp-build" "$tmp/tools"
cp "$root/scripts/whp-build/ensure-automake.sh" "$tmp/water/scripts/whp-build/"
cp "$root/scripts/whp-build/m4.sh" "$tmp/water/scripts/whp-build/"
cp "$root/perl.sh" "$tmp/water/perl.sh"

cat > "$tmp/upstream/bootstrap" <<'EOF'
#!/bin/sh
[ -d .git ] || exit 1
cat > configure <<'EOC'
#!/bin/sh
for opt in "$@"; do
    case "$opt" in
        --prefix=*) printf '%s\n' "${opt#--prefix=}" > .mock-prefix ;;
    esac
done
test -s .mock-prefix
EOC
chmod +x configure
EOF
cat > "$tmp/tools/autoconf" <<'EOF'
#!/bin/sh
[ "${1:-}" = --version ] && echo 'autoconf (GNU Autoconf) mock'
EOF
cat > "$tmp/tools/autom4te" <<'EOF'
#!/bin/sh
[ "${1:-}" = --version ] && echo 'autom4te (GNU Autoconf) mock'
EOF
cat > "$tmp/tools/perl" <<'EOF'
#!/bin/sh
case "${1:-}" in
    -e) printf 'whp-water-perl-ok\n' ;;
    --version) printf 'Mock Perl\nPerl 5\n' ;;
    *) exit 1 ;;
esac
EOF
cat > "$tmp/tools/m4" <<'EOF'
#!/bin/sh
case "$1" in
    --gnu)
        if [ "${2:-}" = --version ]; then
            echo 'm4 (GNU M4) 1.4.20'
        else
            cat >/dev/null
            printf 'whp-water-m4-ok\n'
        fi ;;
    --version) echo 'm4 (GNU M4) 1.4.20' ;;
    *) exit 1 ;;
esac
EOF
cat > "$tmp/tools/makeinfo" <<'EOF'
#!/bin/sh
[ "${1:-}" = --version ] && echo 'makeinfo (GNU texinfo) mock'
EOF
cat > "$tmp/tools/make" <<'EOF'
#!/bin/sh
case "${1:-}" in
    --version) echo 'GNU Make mock' ;;
    install)
        prefix=$(cat .mock-prefix)
        mkdir -p "$prefix/bin"
        cat > "$prefix/bin/automake" <<'EOC'
#!/bin/sh
[ "${1:-}" = --version ] && echo 'automake (GNU automake) mock'
EOC
        cat > "$prefix/bin/aclocal" <<'EOC'
#!/bin/sh
case "${1:-}" in
    --version) echo 'aclocal (GNU automake) mock' ;;
    --print-ac-dir) echo /nonexistent/mock/aclocal ;;
    *) exit 1 ;;
esac
EOC
        chmod +x "$prefix/bin/automake" "$prefix/bin/aclocal"
        printf 'installed\n' >> "$WHP_AUTOMAKE_INSTALL_LOG"
        ;;
esac
EOF
chmod +x "$tmp/upstream/bootstrap" "$tmp/tools/"*
PATH="$tmp/tools:$PATH"
export PATH
git init -q -b master "$tmp/upstream"
git -C "$tmp/upstream" config user.name 'Water test'
git -C "$tmp/upstream" config user.email 'test@example.invalid'
git -C "$tmp/upstream" add -A
git -C "$tmp/upstream" commit -qm 'mock Automake source'
git init -q -b master "$tmp/water"
git -C "$tmp/water" config user.name 'Water test'
git -C "$tmp/water" config user.email 'test@example.invalid'
git -C "$tmp/water" submodule add -q "$tmp/upstream" toolchains/automake
git -C "$tmp/water" add -A
git -C "$tmp/water" commit -qm 'mock Water pin'

export WHP_AUTOMAKE_INSTALL_LOG=$tmp/installs
WHP_AUTOMAKE_SEED_AUTOCONF=$tmp/tools/autoconf
WHP_AUTOMAKE_SEED_AUTOM4TE=$tmp/tools/autom4te
M4=$tmp/tools/m4
PERL=$tmp/tools/perl
MAKE=$tmp/tools/make
export WHP_AUTOMAKE_SEED_AUTOCONF WHP_AUTOMAKE_SEED_AUTOM4TE M4 PERL MAKE
helper=$tmp/water/scripts/whp-build/ensure-automake.sh
prefix=$(/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0)
[ -f "$prefix/.whp-ready" ] && [ -x "$prefix/bin/aclocal" ] || {
    echo 'FAIL: pinned Automake install incomplete' >&2; exit 1
}
/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out"
[ "$(cat "$tmp/out")" = "$prefix" ] &&
[ "$(wc -l < "$WHP_AUTOMAKE_INSTALL_LOG" | tr -d ' ')" -eq 1 ] || {
    echo 'FAIL: healthy Automake cache was rebuilt' >&2; exit 1
}

if PERL=/missing/water-perl /bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out" 2> "$tmp/err"; then
    echo 'FAIL: invalid Perl override reused cached Automake' >&2; exit 1
fi
grep -Fq 'explicit PERL override rejected' "$tmp/err"

# Version mismatch must fail before touching the healthy cache.
cp "$tmp/tools/autom4te" "$tmp/autom4te.saved"
cat > "$tmp/tools/autom4te" <<'EOF'
#!/bin/sh
[ "${1:-}" = --version ] && echo 'autom4te (GNU Autoconf) different'
EOF
chmod +x "$tmp/tools/autom4te"
if /bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out" 2> "$tmp/err"; then
    echo 'FAIL: mismatched seed Autoconf accepted' >&2; exit 1
fi
grep -q 'versions differ' "$tmp/err" || {
    echo 'FAIL: mismatched seed lacks diagnostic' >&2; exit 1
}
mv "$tmp/autom4te.saved" "$tmp/tools/autom4te"

# A broken or mixed Texinfo implementation must be rejected even if the
# old Automake install cache is otherwise valid.
cp "$tmp/tools/makeinfo" "$tmp/makeinfo.saved"
cat > "$tmp/tools/makeinfo" <<'EOF'
#!/bin/sh
[ "${1:-}" = --version ] && echo 'makeinfo (unknown vendor) mock'
EOF
chmod +x "$tmp/tools/makeinfo"
if /bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out" 2> "$tmp/err"; then
    echo 'FAIL: non-GNU makeinfo accepted' >&2; exit 1
fi
grep -q 'makeinfo is not GNU Texinfo' "$tmp/err" || {
    echo 'FAIL: unsupported Texinfo lacks diagnostic' >&2; exit 1
}
mv "$tmp/makeinfo.saved" "$tmp/tools/makeinfo"

rm "$prefix/.whp-ready"
printf 'stale\n' > "$prefix/old"
[ "$(/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0)" = "$prefix" ] &&
[ ! -e "$prefix/old" ] &&
[ "$(wc -l < "$WHP_AUTOMAKE_INSTALL_LOG" | tr -d ' ')" -eq 2 ] || {
    echo 'FAIL: partial Automake cache not repaired' >&2; exit 1
}
rm "$prefix/.whp-ready"
printf 'active\n' > "$prefix/active"
mkdir "$prefix.lock"
if /bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0 > "$tmp/out" 2> "$tmp/err"; then
    echo 'FAIL: bootstrap ignored active lock' >&2; exit 1
fi
[ -f "$prefix/active" ] || {
    echo 'FAIL: locked cache was destroyed' >&2; exit 1
}
rmdir "$prefix.lock"
[ "$(/bin/sh "$helper" "$tmp/water" "$tmp/water/build" 0)" = "$prefix" ] &&
[ ! -e "$prefix/active" ] &&
[ "$(wc -l < "$WHP_AUTOMAKE_INSTALL_LOG" | tr -d ' ')" -eq 3 ] || {
    echo 'FAIL: post-lock cache not recovered' >&2; exit 1
}
echo 'PASS: Automake seed, reuse, mismatch and cache recovery'
