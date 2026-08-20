#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
MODERN_C="$ROOT/src/bootstrap/basaltc.modern.c"
OUT="$ROOT/.tmp/stress"
rm -rf "$OUT"
mkdir -p "$OUT"

GENERATED_SOURCES=()
track_source() { GENERATED_SOURCES+=("$1"); }
cleanup_generated() {
  for source in "${GENERATED_SOURCES[@]}"; do
    rm -f "${source}.c" "${source%.bsl}"
  done
}
trap cleanup_generated EXIT

# Use the stored compiler from the previous Bootstrap generation. Never
# invoke or build the frozen Host.
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$MODERN_C" -o "$OUT/bootstrap.bin"

run_valid() {
  local source=$1 name=$2
  track_source "$source"
  local boot_c="$OUT/${name}.boot.c"
  local boot_bin="$OUT/${name}.boot.bin"
  rm -f "${source}.c" "$boot_c" "$boot_bin"
  "$OUT/bootstrap.bin" "$source" "$boot_c" >/dev/null
  gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  "$boot_bin" >"$OUT/${name}.boot.out"
}

run_negative() {
  local source=$1 name=$2
  track_source "$source"
  rm -f "${source}.c" "$OUT/${name}.boot.c"
  if "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1; then return 1; fi
  test ! -e "${source}.c" && test ! -e "$OUT/${name}.boot.c"
}

count=0
for source in "$ROOT"/tests/stress/case_*.bsl; do
  name=$(basename "$source" .bsl)
  run_valid "$source" "$name"
  count=$((count + 1))
done
for source in "$ROOT"/tests/stress/bad_*.bsl; do
  name=$(basename "$source" .bsl)
  run_negative "$source" "$name"
  count=$((count + 1))
done
run_valid "$ROOT/tests/stress/modulo_stress.bsl" modulo_stress
run_negative "$ROOT/tests/stress/modulo_invalid_string.bsl" modulo_invalid_string
printf 'Bootstrap-only stress suite passed: %d corpus cases plus modulo coverage.\n' "$count"
