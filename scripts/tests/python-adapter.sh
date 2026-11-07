#!/bin/sh
# Probe WATER's generic Python bridge without building Ninja or LLVM.
set -eu
root=$(CDPATH= cd -P -- "$(dirname -- "$0")/../.." && pwd -P)
bridge=$root/python.sh
tmp=$(mktemp -d "${TMPDIR:-/tmp}/water-python-test.XXXXXXXX")
trap 'rm -rf -- "$tmp"' 0
trap 'exit 1' 1 2 3 15
sh -n "$bridge"

real=$(WHP_PYTHON_SEED= PYTHON= sh "$bridge" --print-path) || exit 1
[ -x "$real" ]
[ "$(WHP_PYTHON_SEED="$real" PYTHON= sh "$bridge" --print-path)" = "$real" ]
[ "$(WHP_PYTHON_SEED= PYTHON="$real" sh "$bridge" --print-path)" = "$real" ]
[ "$(WHP_PYTHON_SEED="$real" sh "$bridge" --check)" = "WHP Python interpreter: $real" ]
[ "$(WHP_PYTHON_SEED="$real" sh "$bridge" -c 'import sys; print(":".join(sys.argv[1:]))' alpha 'two words')" = 'alpha:two words' ]

mkdir -p "$tmp/with spaces" "$tmp/bin"
ln -s "$real" "$tmp/with spaces/python"
[ "$(WHP_PYTHON_SEED="$tmp/with spaces/python" sh "$bridge" --print-path)" = "$tmp/with spaces/python" ]

if WHP_PYTHON_SEED="$bridge" sh "$bridge" --print-path > "$tmp/log" 2>&1; then
    echo 'FAIL: Python bridge accepted itself' >&2; exit 1
fi
grep -Fq 'recurses into python.sh' "$tmp/log"
if WHP_PYTHON_SEED=/no/such/python sh "$bridge" --print-path > "$tmp/log" 2>&1; then
    echo 'FAIL: invalid explicit Python seed silently fell back' >&2; exit 1
fi
grep -Fq 'explicit WHP_PYTHON_SEED override rejected' "$tmp/log"
if WHP_PYTHON_SEED= PYTHON=/no/such/python sh "$bridge" --print-path > "$tmp/log" 2>&1; then
    echo 'FAIL: invalid PYTHON override silently fell back' >&2; exit 1
fi
grep -Fq 'explicit PYTHON override rejected' "$tmp/log"
if WHP_PYTHON_SEED='python -I' sh "$bridge" --print-path > "$tmp/log" 2>&1; then
    echo 'FAIL: a Python command with flags was accepted as an executable' >&2; exit 1
fi
grep -Fq 'must be one executable' "$tmp/log"

cat > "$tmp/bin/python" <<'MOCK'
#!/bin/sh
[ "${1:-}" = --version ] && { echo 'Python 3.14.0'; exit 0; }
exit 1
MOCK
chmod +x "$tmp/bin/python"
if WHP_PYTHON_SEED="$tmp/bin/python" sh "$bridge" --print-path > "$tmp/log" 2>&1; then
    echo 'FAIL: a version-only Python stub passed execution validation' >&2; exit 1
fi
grep -Eq 'required standard library failed|probe returned unexpected output' "$tmp/log"
# A failing generic python must allow the next valid candidate, python3.
selected=$(PATH="$tmp/bin:$PATH" WHP_PYTHON_SEED= PYTHON= sh "$bridge" --print-path)
[ "$selected" != "$tmp/bin/python" ] && [ -x "$selected" ] || {
    echo 'FAIL: unusable generic python blocked a valid fallback' >&2; exit 1
}

echo 'PASS: native Python bridge probes runtime/modules, preserves overrides, falls back safely, and forwards arguments'
