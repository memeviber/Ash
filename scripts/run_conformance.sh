#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST="$ROOT/src/compiler/_build/default/bin/basaltc.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
OUT="$ROOT/.tmp/conformance"
rm -rf "$OUT"
mkdir -p "$OUT"
python3 "$ROOT/scripts/generate_longterm_tests.py"

(cd "$ROOT/src/compiler" && dune build bin/basaltc.exe)
(cd "$ROOT/src/compiler" && "$HOST" "$BOOT_SOURCE")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$ROOT/src/bootstrap/basaltc.bsl.c" -o "$OUT/bootstrap.bin"

pass=0
fail=0
for source in "$ROOT"/tests/conformance/*.bsl "$ROOT"/tests/conformance/generated/fuzz_*.bsl "$ROOT"/tests/conformance/generated/lt_valid_*.bsl; do
  [ -f "$source" ] || continue
  name=$(basename "$source" .bsl)
  host_c="$OUT/${name}.host.c"; boot_c="$OUT/${name}.boot.c"
  host_bin="$OUT/${name}.host.bin"; boot_bin="$OUT/${name}.boot.bin"
  if (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1 && cp "${source}.c" "$host_c" && "$OUT/bootstrap.bin" "$source" "$boot_c" >"$OUT/${name}.boot.log" 2>&1 && gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$host_c" -o "$host_bin" && gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin" && "$host_bin" >"$OUT/${name}.host.out" && "$boot_bin" >"$OUT/${name}.boot.out" && cmp -s "$OUT/${name}.host.out" "$OUT/${name}.boot.out"; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1)); echo "FAIL valid $name" >&2
  fi
done
for source in "$ROOT"/tests/conformance/generated/bad_*.bsl; do
  [ -f "$source" ] || continue
  name=$(basename "$source" .bsl)
  rm -f "${source}.c" "$OUT/${name}.boot.c"
  if ! (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1 && ! "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1 && [ ! -e "${source}.c" ] && [ ! -e "$OUT/${name}.boot.c" ]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1)); echo "FAIL negative $name" >&2
  fi
done
printf 'Conformance pass=%d fail=%d\n' "$pass" "$fail"
test "$fail" -eq 0
