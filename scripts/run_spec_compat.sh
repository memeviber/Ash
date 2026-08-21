#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT="$ROOT/.tmp/spec_compat"
STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
source "$ROOT/scripts/bootstrap_stage.sh"

rm -rf "$OUT"
mkdir -p "$OUT"
BOOT_BIN=$(bootstrap_stage "$ROOT" "$OUT/bootstrap-stage" "${STRICT_FLAGS[@]}")

pass=0
fail=0

run_valid() {
  local source=$1
  local name
  name=$(basename "$source" .bsl)
  local generated="$OUT/${name}.c"
  local binary="$OUT/${name}.bin"
  if "$BOOT_BIN" "$source" "$generated" >"$OUT/${name}.compile.log" 2>&1 \
      && gcc "${STRICT_FLAGS[@]}" "$generated" -o "$binary" >"$OUT/${name}.gcc.log" 2>&1 \
      && "$binary" >"$OUT/${name}.run.log" 2>&1; then
    pass=$((pass + 1))
    printf 'PASS valid %s\n' "$name"
  else
    fail=$((fail + 1))
    printf 'FAIL valid %s\n' "$name" >&2
  fi
}

diag_value() {
  local log=$1
  local key=$2
  awk -v key="$key" '$0 == key { if (getline next_line > 0) print next_line; exit }' "$log"
}

check_diag_field() {
  local log=$1
  local key=$2
  local expected=$3
  local actual
  actual=$(diag_value "$log" "$key")
  if [ "$actual" != "$expected" ]; then
    printf 'diagnostic mismatch %s: expected <%s>, found <%s>\n' "$key" "$expected" "$actual" >&2
    return 1
  fi
  return 0
}

check_diag_format() {
  local source=$1
  local log=$2
  local expected_code
  local expected_hint
  local expected_expected
  local expected_found
  local expected_excerpt
  expected_code=$(sed -n 's#^// diagnostic.code: ##p' "$source" | head -1)
  expected_hint=$(sed -n 's#^// diagnostic.hint: ##p' "$source" | head -1)
  expected_expected=$(sed -n 's#^// diagnostic.expected: ##p' "$source" | head -1)
  expected_found=$(sed -n 's#^// diagnostic.found: ##p' "$source" | head -1)
  expected_excerpt=$(sed -n 's#^// diagnostic.excerpt: ##p' "$source" | head -1)
  check_diag_field "$log" diagnostic.file "$source"
  if [ -n "$expected_code" ]; then check_diag_field "$log" diagnostic.code "$expected_code"; fi
  if [ -n "$expected_hint" ]; then check_diag_field "$log" diagnostic.hint "$expected_hint"; fi
  if [ -n "$expected_expected" ]; then check_diag_field "$log" diagnostic.expected "$expected_expected"; fi
  if [ -n "$expected_found" ]; then check_diag_field "$log" diagnostic.found "$expected_found"; fi
  if [ -n "$expected_excerpt" ]; then
    grep -F -q -- "$expected_excerpt" "$log" || {
      printf 'diagnostic mismatch diagnostic.excerpt: expected <%s>\n' "$expected_excerpt" >&2
      return 1
    }
  fi
}

run_invalid() {
  local source=$1
  local name
  name=$(basename "$source" .bsl)
  local generated="$OUT/${name}.c"
  local log="$OUT/${name}.compile.log"
  rm -f "$generated"
  if "$BOOT_BIN" "$source" "$generated" >"$log" 2>&1 || [ -e "$generated" ]; then
    fail=$((fail + 1))
    printf 'FAIL invalid %s\n' "$name" >&2
  elif ! check_diag_format "$source" "$log"; then
    fail=$((fail + 1))
    printf 'FAIL diagnostic %s\n' "$name" >&2
  else
    pass=$((pass + 1))
    printf 'PASS invalid %s\n' "$name"
  fi
}

for source in "$ROOT"/tests/spec/valid/*.bsl; do
  [ -f "$source" ] || continue
  run_valid "$source"
done
for source in "$ROOT"/tests/spec/invalid/*.bsl; do
  [ -f "$source" ] || continue
  run_invalid "$source"
done

printf 'Bootstrap specification compatibility pass=%d fail=%d\n' "$pass" "$fail"
test "$fail" -eq 0
