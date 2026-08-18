#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
COMPILER="$ROOT/src/compiler/_build/default/bin/main.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/ashc.ash"
BOOT_C="$ROOT/src/bootstrap/ashc.ash.c"
BOOT_BIN="$ROOT/.tmp/bootstrap.bin"
OUT="$ROOT/.tmp/regression"
mkdir -p "$OUT"

(cd "$ROOT/src/compiler" && dune build bin/main.exe)
(cd "$ROOT/src/compiler" && "$COMPILER" "$BOOT_SOURCE")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$BOOT_C" -o "$BOOT_BIN"

compile_run() {
  local source=$1 label=$2
  local host_c="$OUT/${label}.host.c" boot_c="$OUT/${label}.boot.c"
  local host_bin="$OUT/${label}.host.bin" boot_bin="$OUT/${label}.boot.bin"
  rm -f "$host_c" "$boot_c" "$host_bin" "$boot_bin" "$ROOT"/$(basename "$source").c
  (cd "$(dirname "$source")" && "$COMPILER" "$(basename "$source")") >/dev/null
  cp "${source}.c" "$host_c"
  "$BOOT_BIN" "$source" "$boot_c" >/dev/null
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$host_c" -o "$host_bin"
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  "$host_bin"
  "$boot_bin"
  printf 'PASS %s\n' "$label"
}

expect_reject() {
  local source=$1 label=$2
  local host_log="$OUT/${label}.host.log" boot_log="$OUT/${label}.boot.log"
  rm -f "${source}.c" "$OUT/${label}.boot.c"
  if (cd "$(dirname "$source")" && "$COMPILER" "$(basename "$source")") >"$host_log" 2>&1; then
    echo "FAIL $label: Host accepted invalid source" >&2
    return 1
  fi
  if "$BOOT_BIN" "$source" "$OUT/${label}.boot.c" >"$boot_log" 2>&1; then
    echo "FAIL $label: Bootstrap accepted invalid source" >&2
    return 1
  fi
  test ! -e "${source}.c" && test ! -e "$OUT/${label}.boot.c"
  printf 'PASS %s (rejected)\n' "$label"
}

compile_run "$ROOT/test/stress/modulo_stress.ash" modulo_stress
expect_reject "$ROOT/test/stress/modulo_invalid_string.ash" modulo_invalid_string
printf 'Regression checks completed successfully.\n'
