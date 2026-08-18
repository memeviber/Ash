#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST="$ROOT/src/compiler/_build/default/bin/basaltc.exe"
BOOT_C_SOURCE="$ROOT/src/bootstrap/basaltc.basalt.c"
OUT="$ROOT/.tmp/ownership_stress"

rm -rf "$OUT"
mkdir -p "$OUT"

printf '%s\n' '[1/3] Building Host and Bootstrap.'
(cd "$ROOT/src/compiler" && dune build bin/basaltc.exe)
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
  "$BOOT_C_SOURCE" -o "$OUT/bootstrap.bin"

run_valid() {
  local source=$1
  local name
  name=$(basename "$source" .basalt)
  local dir="$OUT/$name"
  mkdir -p "$dir"
  cp "$source" "$dir/$name.basalt"

  if ! (cd "$dir" && "$HOST" "$name.basalt") >"$dir/host.compile.out" 2>"$dir/host.compile.err"; then
    printf 'FAIL %s: Host rejected valid fixture\n' "$name" >&2
    return 1
  fi
  if ! "$OUT/bootstrap.bin" "$dir/$name.basalt" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    printf 'FAIL %s: Bootstrap rejected valid fixture\n' "$name" >&2
    return 1
  fi

  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
    "$dir/$name.basalt.c" -o "$dir/host.bin" >"$dir/host.gcc.out" 2>"$dir/host.gcc.err"
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
    "$dir/$name.boot.c" -o "$dir/bootstrap.bin" >"$dir/bootstrap.gcc.out" 2>"$dir/bootstrap.gcc.err"
  "$dir/host.bin" >"$dir/host.run.out" 2>"$dir/host.run.err"
  "$dir/bootstrap.bin" >"$dir/bootstrap.run.out" 2>"$dir/bootstrap.run.err"
  if ! cmp -s "$dir/host.run.out" "$dir/bootstrap.run.out"; then
    printf 'FAIL %s: Host/Bootstrap runtime output differs\n' "$name" >&2
    diff -u "$dir/host.run.out" "$dir/bootstrap.run.out" >&2 || true
    return 1
  fi

  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
    -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$dir/$name.basalt.c" -o "$dir/host.san.bin"
  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
    -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$dir/$name.boot.c" -o "$dir/bootstrap.san.bin"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$dir/host.san.bin" >"$dir/host.san.out" 2>"$dir/host.san.err"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$dir/bootstrap.san.bin" >"$dir/bootstrap.san.out" 2>"$dir/bootstrap.san.err"
  cmp -s "$dir/host.san.out" "$dir/bootstrap.san.out"
  if grep -Eqi 'ERROR:|runtime error:|LeakSanitizer|AddressSanitizer|UndefinedBehaviorSanitizer' \
      "$dir/host.san.err" "$dir/bootstrap.san.err"; then
    printf 'FAIL %s: sanitizer diagnostics detected\n' "$name" >&2
    return 1
  fi
  printf 'PASS %s (accepted, strict GCC, sanitizer, runtime parity)\n' "$name"
}

run_invalid() {
  local source=$1
  local name
  name=$(basename "$source" .basalt)
  local dir="$OUT/$name"
  mkdir -p "$dir"
  cp "$source" "$dir/$name.basalt"

  local host_status=0
  if (cd "$dir" && "$HOST" "$name.basalt") >"$dir/host.compile.out" 2>"$dir/host.compile.err"; then
    host_status=0
  else
    host_status=$?
  fi
  local bootstrap_status=0
  if "$OUT/bootstrap.bin" "$dir/$name.basalt" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    bootstrap_status=0
  else
    bootstrap_status=$?
  fi
  if [ "$host_status" -eq 0 ] || [ "$bootstrap_status" -eq 0 ]; then
    printf 'FAIL %s: acceptance mismatch (Host=%s Bootstrap=%s)\n' "$name" "$host_status" "$bootstrap_status" >&2
    return 1
  fi
  printf 'PASS %s (rejected by both; Host=%s Bootstrap=%s)\n' "$name" "$host_status" "$bootstrap_status"
}

printf '%s\n' '[2/3] Running ownership fixtures.'
run_valid "$ROOT/tests/stress/move_borrow_valid.basalt"
for source in "$ROOT"/tests/stress/move_borrow_invalid_*.basalt; do
  run_invalid "$source"
done

printf '%s\n' '[3/3] Running existing suites.'
run_suite() {
  local label=$1
  local script=$2
  if ! bash "$ROOT/scripts/$script" >"$OUT/$label.log" 2>&1; then
    printf 'FAIL %s; see %s\n' "$label" "$OUT/$label.log" >&2
    return 1
  fi
  printf 'PASS %s\n' "$label"
}
run_suite regression run_regression.sh
run_suite stress run_stress.sh
run_suite adversarial run_adversarial.sh
run_suite conformance run_conformance.sh
run_suite fixed_point fixed_point.sh

printf 'Ownership stress suite completed successfully. Artifacts: %s\n' "$OUT"
