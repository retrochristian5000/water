#!/bin/sh
# Check native Perl selection independently of a full Autotools bootstrap.
set -eu
root=$(CDPATH= cd -P -- "$(dirname -- "$0")/../.." && pwd -P)
bridge=$root/perl.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-perl-test.XXXXXXXX")
trap 'rm -rf -- "$tmp"' 0
trap 'exit 1' 1 2 3 15
sh -n "$bridge"

real=$(PERL= sh "$bridge" --print-path) || exit 1
[ -x "$real" ]
[ "$(PERL="$real" sh "$bridge" --print-path)" = "$real" ]
[ "$(PERL="$real" sh "$bridge" --check)" = "WHP Perl interpreter: $real" ]
[ "$(PERL="$real" sh "$bridge" -e 'print join(q{:}, @ARGV)' alpha 'two words')" = 'alpha:two words' ]

mkdir -p "$tmp/with spaces" "$tmp/bin"
ln -s "$real" "$tmp/with spaces/perl"
[ "$(PERL="$tmp/with spaces/perl" sh "$bridge" --print-path)" = "$tmp/with spaces/perl" ]

if PERL="$bridge" sh "$bridge" --print-path >"$tmp/log" 2>&1; then
    echo 'FAIL: Perl adapter accepted itself' >&2; exit 1
fi
grep -Fq 'recurses into perl.sh' "$tmp/log"
if PERL=/no/such/perl sh "$bridge" --print-path >"$tmp/log" 2>&1; then
    echo 'FAIL: missing explicit Perl silently fell back' >&2; exit 1
fi
grep -Fq 'not executable' "$tmp/log"
if PERL='perl -w' sh "$bridge" --print-path >"$tmp/log" 2>&1; then
    echo 'FAIL: multiword Perl command accepted' >&2; exit 1
fi
grep -Fq 'must be one executable' "$tmp/log"

cat > "$tmp/bin/perl" <<'MOCK'
#!/bin/sh
[ "${1:-}" = --version ] && printf 'Perl 5.40\n'
exit 1
MOCK
chmod +x "$tmp/bin/perl"
if PERL="$tmp/bin/perl" sh "$bridge" --print-path >"$tmp/log" 2>&1; then
    echo 'FAIL: version-only stub passed real Perl execution probe' >&2; exit 1
fi
grep -Fq 'required core modules failed' "$tmp/log"
ln -s "$real" "$tmp/bin/perl5"
[ "$(PATH="$tmp/bin:$PATH" PERL= sh "$bridge" --print-path)" = "$tmp/bin/perl5" ]
[ "$(PATH="$tmp/bin:$PATH" PERL="$real" sh "$bridge" --print-path)" = "$real" ]

echo 'PASS: Perl bridge selects a runnable interpreter, isolates invalid overrides, and forwards arguments'
