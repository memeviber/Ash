#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
COMPILER="$ROOT/src/compiler/_build/default/bin/basaltc.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
BOOT_C="$ROOT/src/bootstrap/basaltc.bsl.c"
BOOT_BIN="$ROOT/.tmp/bootstrap.bin"
OUT="$ROOT/.tmp/regression"
mkdir -p "$OUT"

GENERATED_SOURCES=()
track_source() { GENERATED_SOURCES+=("$1"); }
cleanup_generated() {
  rm -f "$ROOT/src/bootstrap/basaltc"
  for source in "${GENERATED_SOURCES[@]}"; do
    rm -f "${source}.c" "${source%.bsl}"
  done
}
trap cleanup_generated EXIT

(cd "$ROOT/src/compiler" && dune build bin/basaltc.exe)
(cd "$ROOT/src/compiler" && "$COMPILER" "$BOOT_SOURCE")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$BOOT_C" -o "$BOOT_BIN"

assert_single_runtime_prologue() {
  local c_file=$1 label=$2
  local prefix needle count
  prefix=$(awk '/^static void\* basalt_track\(void\*\);/{print; exit} {print}' "$c_file")
  for needle in '#include <stdio.h>' '#include <stdlib.h>' '#include <string.h>' '_POSIX_C_SOURCE 200809L' '_XOPEN_SOURCE 700'; do
    count=$(printf '%s\n' "$prefix" | grep -F -c "$needle" || true)
    if [[ "$count" -ne 1 ]]; then
      echo "FAIL $label: expected exactly one '$needle' in generated C prologue, found $count" >&2
      return 1
    fi
  done
  if printf '%s\n' "$prefix" | grep -Fq '#if !defined(_WIN32)'; then
    echo "FAIL $label: obsolete duplicate feature prelude remains in generated C" >&2
    return 1
  fi
}

compile_run() {
  local source=$1 label=$2
  track_source "$source"
  local host_c="$OUT/${label}.host.c" boot_c="$OUT/${label}.boot.c"
  local host_bin="$OUT/${label}.host.bin" boot_bin="$OUT/${label}.boot.bin"
  rm -f "$host_c" "$boot_c" "$host_bin" "$boot_bin" "$ROOT"/$(basename "$source").c
  (cd "$(dirname "$source")" && "$COMPILER" "$(basename "$source")") >/dev/null
  cp "${source}.c" "$host_c"
  "$BOOT_BIN" "$source" "$boot_c" >/dev/null
  if [[ "$label" == "include_test_main" ]]; then
    assert_single_runtime_prologue "$host_c" "$label (Host)"
    assert_single_runtime_prologue "$boot_c" "$label (Bootstrap)"
  fi
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$host_c" -o "$host_bin"
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  "$host_bin"
  "$boot_bin"
  printf 'PASS %s\n' "$label"
}

expect_reject() {
  local source=$1 label=$2
  track_source "$source"
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

expect_collision_reject() {
  local source=$1 label=$2
  expect_reject "$source" "$label"
  grep -Fq 'C symbol collision' "$OUT/${label}.host.log"
  grep -Fq 'distinct functions collide after C mangling' "$OUT/${label}.boot.log"
}

compile_run "$ROOT/tests/stress/modulo_stress.bsl" modulo_stress
compile_run "$ROOT/tests/regression/stdlib_growth_test.bsl" stdlib_growth_test
compile_run "$ROOT/tests/regression/stdlib_containers_test.bsl" stdlib_containers_test
compile_run "$ROOT/tests/regression/option_test.bsl" option_test
compile_run "$ROOT/tests/regression/builtin_join_test.bsl" builtin_join_test
compile_run "$ROOT/tests/regression/stdlib_slice_only_test.bsl" stdlib_slice_only_test
compile_run "$ROOT/tests/regression/stdlib_map_only_test.bsl" stdlib_map_only_test
compile_run "$ROOT/tests/regression/stdlib_hashing_test.bsl" stdlib_hashing_test
compile_run "$ROOT/tests/regression/map_bucket_mask_edge.bsl" map_bucket_mask_edge
compile_run "$ROOT/tests/stress/map_bucket_mask_stress.bsl" map_bucket_mask_stress
compile_run "$ROOT/tests/regression/stress_containers_loop.bsl" stress_containers_loop
compile_run "$ROOT/tests/regression/generic_map_probe.bsl" generic_map_probe
compile_run "$ROOT/tests/regression/include_test_main.bsl" include_test_main
compile_run "$ROOT/tests/regression/print_pointer_test.bsl" print_pointer_test
grep -Fq '%p' "$OUT/print_pointer_test.host.c"
grep -Fq '(void*)' "$OUT/print_pointer_test.host.c"
grep -Fq '%p' "$OUT/print_pointer_test.boot.c"
grep -Fq '(void*)' "$OUT/print_pointer_test.boot.c"
printf 'PASS print_pointer_test format guard\n'
compile_run "$ROOT/tests/regression/namespace_collision.bsl" namespace_collision
compile_run "$ROOT/tests/regression/nested_namespace_valid.bsl" nested_namespace_valid
compile_run "$ROOT/tests/regression/namespace_global.bsl" namespace_global
compile_run "$ROOT/tests/regression/pointer_struct_field.bsl" pointer_struct_field
compile_run "$ROOT/tests/regression/pointer_generic_struct_test.bsl" pointer_generic_struct_test
compile_run "$ROOT/tests/regression/fixed_array_valid.bsl" fixed_array_valid
expect_reject "$ROOT/tests/regression/fixed_array_oob_literal.bsl" fixed_array_oob_literal
expect_reject "$ROOT/tests/regression/fixed_array_oob_negative.bsl" fixed_array_oob_negative
compile_run "$ROOT/tests/regression/enum_typechecker_valid.bsl" enum_typechecker_valid
expect_reject "$ROOT/tests/regression/enum_typechecker_invalid.bsl" enum_typechecker_invalid
expect_reject "$ROOT/tests/regression/fixed_array_oob_struct_field.bsl" fixed_array_oob_struct_field
expect_collision_reject "$ROOT/tests/regression/mangle_collision.bsl" mangle_collision
expect_collision_reject "$ROOT/tests/regression/nested_namespace_flat_collision.bsl" nested_namespace_flat_collision
expect_collision_reject "$ROOT/tests/regression/nested_namespace_segment_collision.bsl" nested_namespace_segment_collision
expect_reject "$ROOT/tests/stress/modulo_invalid_string.bsl" modulo_invalid_string
expect_reject "$ROOT/tests/regression/undefined_function_call.bsl" undefined_function_call
expect_reject "$ROOT/tests/regression/non_function_value_call.bsl" non_function_value_call
expect_reject "$ROOT/tests/regression/unknown_variable_use.bsl" unknown_variable_use
expect_reject "$ROOT/tests/regression/unknown_field_access.bsl" unknown_field_access
expect_reject "$ROOT/tests/regression/deref_non_pointer.bsl" deref_non_pointer
expect_reject "$ROOT/tests/regression/index_non_container.bsl" index_non_container
expect_reject "$ROOT/tests/regression/indirect_call_non_function.bsl" indirect_call_non_function
expect_reject "$ROOT/tests/regression/reserved_runtime_function.bsl" reserved_runtime_function
printf 'Regression checks completed successfully.\n'
