#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST="$ROOT/src/compiler/_build/default/bin/basaltc.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.basalt"
BOOT_C="$ROOT/src/bootstrap/basaltc.basalt.c"
OUT="$ROOT/.tmp/stress"
rm -rf "$OUT"
mkdir -p "$OUT"

(cd "$ROOT/src/compiler" && dune build bin/basaltc.exe)
(cd "$ROOT/src/compiler" && "$HOST" "$BOOT_SOURCE")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$BOOT_C" -o "$OUT/bootstrap.bin"

run_valid() {
  local source=$1 name=$2
  local host_c="$OUT/${name}.host.c" boot_c="$OUT/${name}.boot.c"
  local host_bin="$OUT/${name}.host.bin" boot_bin="$OUT/${name}.boot.bin"
  rm -f "${source}.c" "$host_c" "$boot_c" "$host_bin" "$boot_bin"
  (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >/dev/null
  cp "${source}.c" "$host_c"
  "$OUT/bootstrap.bin" "$source" "$boot_c" >/dev/null
  gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$host_c" -o "$host_bin"
  gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  "$host_bin" >"$OUT/${name}.host.out"
  "$boot_bin" >"$OUT/${name}.boot.out"
  cmp -s "$OUT/${name}.host.out" "$OUT/${name}.boot.out"
}

run_negative() {
  local source=$1 name=$2
  rm -f "${source}.c" "$OUT/${name}.boot.c"
  if (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1; then return 1; fi
  if "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1; then return 1; fi
  test ! -e "${source}.c" && test ! -e "$OUT/${name}.boot.c"
}

count=0
for source in "$ROOT"/tests/stress/case_*.basalt; do
  name=$(basename "$source" .basalt)
  run_valid "$source" "$name"
  count=$((count + 1))
done
for source in "$ROOT"/tests/stress/bad_*.basalt; do
  name=$(basename "$source" .basalt)
  run_negative "$source" "$name"
  count=$((count + 1))
done
run_valid "$ROOT/tests/stress/modulo_stress.basalt" modulo_stress
run_negative "$ROOT/tests/stress/modulo_invalid_string.basalt" modulo_invalid_string
printf 'Stress suite passed: %d corpus cases plus modulo coverage.\n' "$count"
