#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST="$ROOT/src/compiler/_build/default/bin/main.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/pyrelc.pyrel"
OUT="$ROOT/.tmp/conformance"
rm -rf "$OUT"
mkdir -p "$OUT"

(cd "$ROOT/src/compiler" && dune build bin/main.exe)
(cd "$ROOT/src/compiler" && "$HOST" "$BOOT_SOURCE")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow "$ROOT/src/bootstrap/pyrelc.pyrel.c" -o "$OUT/bootstrap.bin"

pass=0
fail=0
for source in "$ROOT"/tests/conformance/*.pyrel "$ROOT"/tests/conformance/generated/fuzz_*.pyrel; do
  [ -f "$source" ] || continue
  name=$(basename "$source" .pyrel)
  host_c="$OUT/${name}.host.c"; boot_c="$OUT/${name}.boot.c"
  host_bin="$OUT/${name}.host.bin"; boot_bin="$OUT/${name}.boot.bin"
  if (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1 && cp "${source}.c" "$host_c" && "$OUT/bootstrap.bin" "$source" "$boot_c" >"$OUT/${name}.boot.log" 2>&1 && gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow "$host_c" -o "$host_bin" && gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow "$boot_c" -o "$boot_bin" && "$host_bin" >"$OUT/${name}.host.out" && "$boot_bin" >"$OUT/${name}.boot.out" && cmp -s "$OUT/${name}.host.out" "$OUT/${name}.boot.out"; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1)); echo "FAIL valid $name" >&2
  fi
done
for source in "$ROOT"/tests/conformance/generated/bad_*.pyrel; do
  [ -f "$source" ] || continue
  name=$(basename "$source" .pyrel)
  rm -f "${source}.c" "$OUT/${name}.boot.c"
  if ! (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1 && ! "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1 && [ ! -e "${source}.c" ] && [ ! -e "$OUT/${name}.boot.c" ]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1)); echo "FAIL negative $name" >&2
  fi
done
printf 'Conformance pass=%d fail=%d\n' "$pass" "$fail"
test "$fail" -eq 0
