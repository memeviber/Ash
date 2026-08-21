#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.basalt"
OUT="$ROOT/.tmp/stress"
source "$ROOT/scripts/bootstrap_stage.sh"
STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
rm -rf "$OUT"
mkdir -p "$OUT"

GENERATED_SOURCES=()
track_source() { GENERATED_SOURCES+=("$1"); }
cleanup_generated() {
  for source in "${GENERATED_SOURCES[@]}"; do
    rm -f "${source}.c" "${source%.basalt}"
  done
}
trap cleanup_generated EXIT

# The frozen C compiler creates the current-generation Bootstrap compiler.
BOOT_BIN=$(bootstrap_stage "$ROOT" "$OUT" "${STRICT_FLAGS[@]}")

run_valid() {
  local source=$1 name=$2
  track_source "$source"
  local boot_c="$OUT/${name}.boot.c"
  local boot_bin="$OUT/${name}.boot.bin"
  rm -f "${source}.c" "$boot_c" "$boot_bin"
  "$BOOT_BIN" "$source" "$boot_c" >/dev/null
  gcc -std=c11 -O1 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  "$boot_bin" >"$OUT/${name}.boot.out"
}

run_negative() {
  local source=$1 name=$2
  track_source "$source"
  rm -f "${source}.c" "$OUT/${name}.boot.c"
  if "$BOOT_BIN" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1; then return 1; fi
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
printf 'Bootstrap-only stress suite passed: %d corpus cases plus modulo coverage.\n' "$count"
