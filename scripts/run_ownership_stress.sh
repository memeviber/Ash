#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODERN_C="$ROOT/src/bootstrap/basaltc.modern.c"
OUT="$ROOT/.tmp/ownership_stress"

rm -rf "$OUT"
mkdir -p "$OUT"

printf '%s\n' '[1/3] Building the stored Bootstrap compiler.'
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
  "$MODERN_C" -o "$OUT/bootstrap.bin"

check_no_stray_binaries() {
  local stray
  stray=$(find "$ROOT/tests" -type f -print0 | xargs -0 file 2>/dev/null | grep -F 'ELF ' || true)
  if [ -n "$stray" ]; then
    printf 'FAIL: stray ELF binaries under tests/ (all executables must live in .tmp/):\n%s\n' "$stray" >&2
    return 1
  fi
  printf 'PASS no stray ELF binaries under tests/\n'
}

run_valid() {
  local source=$1
  local name
  name=$(basename "$source" .bsl)
  local dir="$OUT/$name"
  mkdir -p "$dir"
  cp "$source" "$dir/$name.bsl"

  if ! "$OUT/bootstrap.bin" "$dir/$name.bsl" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    printf 'FAIL %s: Bootstrap rejected valid fixture\n' "$name" >&2
    return 1
  fi

  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
    "$dir/$name.boot.c" -o "$dir/bootstrap.bin" >"$dir/bootstrap.gcc.out" 2>"$dir/bootstrap.gcc.err"
  "$dir/bootstrap.bin" >"$dir/bootstrap.run.out" 2>"$dir/bootstrap.run.err"

  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow \
    -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    "$dir/$name.boot.c" -o "$dir/bootstrap.san.bin"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$dir/bootstrap.san.bin" >"$dir/bootstrap.san.out" 2>"$dir/bootstrap.san.err"
  if grep -Eqi 'ERROR:|runtime error:|LeakSanitizer|AddressSanitizer|UndefinedBehaviorSanitizer' \
      "$dir/bootstrap.san.err"; then
    printf 'FAIL %s: sanitizer diagnostics detected\n' "$name" >&2
    return 1
  fi
  printf 'PASS %s (Bootstrap accepted, strict GCC, sanitizer)\n' "$name"
}

run_invalid() {
  local source=$1
  local name
  name=$(basename "$source" .bsl)
  local dir="$OUT/$name"
  mkdir -p "$dir"
  cp "$source" "$dir/$name.bsl"

  if "$OUT/bootstrap.bin" "$dir/$name.bsl" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    printf 'FAIL %s: Bootstrap accepted invalid fixture\n' "$name" >&2
    return 1
  fi
  test ! -e "$dir/$name.boot.c"
  printf 'PASS %s (Bootstrap rejected)\n' "$name"
}

printf '%s\n' '[2/3] Running ownership fixtures.'
run_valid "$ROOT/tests/stress/move_borrow_valid.bsl"
for source in "$ROOT"/tests/stress/move_borrow_invalid_*.bsl; do
  run_invalid "$source"
done

printf '%s\n' '[3/3] Running Bootstrap-only suites.'
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
if ! python3 "$ROOT/scripts/run_code_buffer_v3.py" >"$OUT/code_buffer_v3.log" 2>&1; then
  printf 'FAIL code_buffer_v3; see %s\n' "$OUT/code_buffer_v3.log" >&2
  exit 1
fi
printf 'PASS code_buffer_v3\n'
check_no_stray_binaries

printf 'Bootstrap-only ownership stress suite completed successfully. Artifacts: %s\n' "$OUT"
