#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
OUT="$ROOT/.tmp/conformance"
GENERATED="$ROOT/tests/conformance/generated"
source "$ROOT/scripts/bootstrap_stage.sh"
STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
rm -rf "$OUT"
mkdir -p "$OUT"

GENERATED_SOURCES=()
track_source() { GENERATED_SOURCES+=("$1"); }
cleanup_generated() {
  for source in "$GENERATED"/lt_valid_*.bsl "$GENERATED"/bad_lt_*.bsl; do
    [ -f "$source" ] || continue
    rel=${source#"$ROOT/"}
    if ! git -C "$ROOT" ls-files --error-unmatch -- "$rel" >/dev/null 2>&1; then
      rm -f "$source"
    fi
  done
  for source in "${GENERATED_SOURCES[@]}"; do
    rm -f "${source}.c" "${source%.bsl}"
  done
}
trap cleanup_generated EXIT

python3 "$ROOT/scripts/generate_longterm_tests.py"

# The frozen C compiler creates the current-generation Bootstrap compiler.
BOOT_BIN=$(bootstrap_stage "$ROOT" "$OUT" "${STRICT_FLAGS[@]}")

pass=0
fail=0
for source in "$ROOT"/tests/conformance/*.bsl "$ROOT"/tests/conformance/generated/fuzz_*.bsl "$ROOT"/tests/conformance/generated/lt_valid_*.bsl; do
  [ -f "$source" ] || continue
  name=$(basename "$source" .bsl)
  track_source "$source"
  boot_c="$OUT/${name}.boot.c"
  boot_bin="$OUT/${name}.boot.bin"
  if "$BOOT_BIN" "$source" "$boot_c" >"$OUT/${name}.boot.log" 2>&1 && gcc "${STRICT_FLAGS[@]}" "$boot_c" -o "$boot_bin" && "$boot_bin" >"$OUT/${name}.boot.out"; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1)); echo "FAIL valid $name" >&2
  fi
done
for source in "$ROOT"/tests/conformance/generated/bad_*.bsl; do
  [ -f "$source" ] || continue
  name=$(basename "$source" .bsl)
  track_source "$source"
  rm -f "${source}.c" "$OUT/${name}.boot.c"
  if ! "$BOOT_BIN" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1 && [ ! -e "${source}.c" ] && [ ! -e "$OUT/${name}.boot.c" ]; then
    pass=$((pass + 1))
  else
    fail=$((fail + 1)); echo "FAIL negative $name" >&2
  fi
done
printf 'Bootstrap-only conformance pass=%d fail=%d\n' "$pass" "$fail"
test "$fail" -eq 0
