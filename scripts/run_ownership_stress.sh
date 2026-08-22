#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT="$ROOT/.tmp/ownership_stress"
source "$ROOT/scripts/bootstrap_stage.sh"
STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
SAN_FLAGS=(-std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer)

rm -rf "$OUT"
mkdir -p "$OUT"

printf '%s\n' '[1/3] Building the current Bootstrap compiler from the frozen C seed.'
BOOT_BIN=$(bootstrap_stage "$ROOT" "$OUT" "${STRICT_FLAGS[@]}")

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
  name=$(basename "$source" .basalt)
  local dir="$OUT/$name"
  mkdir -p "$dir"
  cp "$source" "$dir/$name.basalt"

  if ! "$BOOT_BIN" "$dir/$name.basalt" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    printf 'FAIL %s: Bootstrap rejected valid fixture\n' "$name" >&2
    return 1
  fi

  gcc "${STRICT_FLAGS[@]}" "$dir/$name.boot.c" -o "$dir/bootstrap.bin" >"$dir/bootstrap.gcc.out" 2>"$dir/bootstrap.gcc.err"
  "$dir/bootstrap.bin" >"$dir/bootstrap.run.out" 2>"$dir/bootstrap.run.err"

  gcc "${SAN_FLAGS[@]}" "$dir/$name.boot.c" -o "$dir/bootstrap.san.bin"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
    "$dir/bootstrap.san.bin" >"$dir/bootstrap.san.out" 2>"$dir/bootstrap.san.err"
  if grep -Eqi 'ERROR:|runtime error:|LeakSanitizer|AddressSanitizer|UndefinedBehaviorSanitizer' \
      "$dir/bootstrap.san.err"; then
    printf 'FAIL %s: sanitizer diagnostics detected\n' "$name" >&2
    return 1
  fi
  printf 'PASS %s (Bootstrap accepted, strict GCC, sanitizer)\n' "$name"
}

run_stdlib_valid() {
  local source=$1
  local name
  name=$(basename "$source" .basalt)
  local dir="$OUT/$name"
  mkdir -p "$dir"

  if ! "$BOOT_BIN" "$source" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    printf 'FAIL %s: Bootstrap rejected stdlib fixture\n' "$name" >&2
    return 1
  fi

  gcc "${STRICT_FLAGS[@]}" -pthread "$dir/$name.boot.c" -o "$dir/strict.bin" \
    >"$dir/strict.gcc.out" 2>"$dir/strict.gcc.err"
  (cd "$ROOT" && "$dir/strict.bin" >"$dir/strict.run.out" 2>"$dir/strict.run.err")

  gcc "${SAN_FLAGS[@]}" -pthread "$dir/$name.boot.c" -o "$dir/san.bin" \
    >"$dir/san.gcc.out" 2>"$dir/san.gcc.err"
  if ! (cd "$ROOT" && ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
      "$dir/san.bin" >"$dir/san.run.out" 2>"$dir/san.run.err"); then
    printf 'FAIL %s: sanitizer execution failed\n' "$name" >&2
    return 1
  fi
  if grep -Eqi 'ERROR:|runtime error:|LeakSanitizer|AddressSanitizer|UndefinedBehaviorSanitizer' \
      "$dir/san.run.err"; then
    printf 'FAIL %s: sanitizer diagnostics detected\n' "$name" >&2
    return 1
  fi
  printf 'PASS %s (stdlib strict GCC, sanitizer)\n' "$name"
}

run_invalid() {
  local source=$1
  local name
  name=$(basename "$source" .basalt)
  local dir="$OUT/$name"
  mkdir -p "$dir"
  cp "$source" "$dir/$name.basalt"

  if "$BOOT_BIN" "$dir/$name.basalt" "$dir/$name.boot.c" >"$dir/bootstrap.compile.out" 2>"$dir/bootstrap.compile.err"; then
    printf 'FAIL %s: Bootstrap accepted invalid fixture\n' "$name" >&2
    return 1
  fi
  test ! -e "$dir/$name.boot.c"
  printf 'PASS %s (Bootstrap rejected)\n' "$name"
}

printf '%s\n' '[2/3] Running ownership fixtures.'
run_valid "$ROOT/tests/stress/move_borrow_valid.basalt"
run_valid "$ROOT/tests/spec/valid/ownership_lifetime_valid.basalt"
run_valid "$ROOT/tests/spec/valid/rust_borrow_valid.basalt"
run_valid "$ROOT/tests/spec/valid/rust_borrow_flow_valid.basalt"
run_valid "$ROOT/tests/spec/valid/rust_borrow_loop_move_valid.basalt"
run_valid "$ROOT/tests/stress/case_borrow_lexical_nested.basalt"
run_valid "$ROOT/tests/stress/case_borrow_alias_rebind.basalt"
run_valid "$ROOT/tests/stress/case_borrow_branch_loop.basalt"
run_valid "$ROOT/tests/stress/case_borrow_pointer_depth.basalt"
run_valid "$ROOT/tests/stress/case_borrow_return_chain.basalt"
run_stdlib_valid "$ROOT/tests/regression/stdlib_filesystem_path_string_test.basalt"
run_stdlib_valid "$ROOT/tests/regression/stdlib_utf8_test.basalt"
run_stdlib_valid "$ROOT/tests/regression/stdlib_time_process_format_random_test.basalt"
run_stdlib_valid "$ROOT/tests/regression/stdlib_concurrency_extended_test.basalt"
run_stdlib_valid "$ROOT/tests/regression/string_builder_iter_test.basalt"
for source in "$ROOT"/tests/stress/move_borrow_invalid_*.basalt; do
  run_invalid "$source"
done
for source in \
  "$ROOT/tests/spec/invalid/rust_shared_then_mut_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_mutate_shared_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_double_mut_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_mut_reborrow_conflict_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_mut_const_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_borrow_temporary_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_return_local_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_return_mixed_lifetime_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_move_while_borrowed_invalid.basalt" \
  "$ROOT/tests/spec/invalid/rust_flow_shared_then_mut_invalid.basalt" \
  "$ROOT/tests/stress/bad_borrow_loop_conflict.basalt" \
  "$ROOT/tests/stress/bad_borrow_branch_join_conflict.basalt" \
  "$ROOT/tests/stress/bad_borrow_global_const.basalt" \
  "$ROOT/tests/stress/bad_borrow_sibling_reborrow.basalt" \
  "$ROOT/tests/stress/bad_borrow_return_mixed_chain.basalt"; do
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
run_suite spec_compat run_spec_compat.sh
run_suite fixed_point fixed_point.sh
if ! python3 "$ROOT/scripts/run_code_buffer_v3.py" >"$OUT/code_buffer_v3.log" 2>&1; then
  printf 'FAIL code_buffer_v3; see %s\n' "$OUT/code_buffer_v3.log" >&2
  exit 1
fi
printf 'PASS code_buffer_v3\n'
if ! python3 "$ROOT/scripts/run_memory_sanitizer.py" >"$OUT/memory_sanitizer.log" 2>&1; then
  printf 'FAIL memory_sanitizer; see %s\n' "$OUT/memory_sanitizer.log" >&2
  exit 1
fi
printf 'PASS memory_sanitizer\n'
check_no_stray_binaries

printf 'Bootstrap-only ownership stress suite completed successfully. Artifacts: %s\n' "$OUT"
