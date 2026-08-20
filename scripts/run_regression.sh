#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
BOOT_BIN="$ROOT/.tmp/bootstrap.bin"
OUT="$ROOT/.tmp/regression"
source "$ROOT/scripts/bootstrap_stage.sh"
STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
mkdir -p "$OUT"

GENERATED_SOURCES=()
track_source() { GENERATED_SOURCES+=("$1"); }
cleanup_generated() {
  for source in "${GENERATED_SOURCES[@]}"; do
    rm -f "${source}.c" "${source%.bsl}"
  done
}
trap cleanup_generated EXIT

# The stored C compiler builds the current Bootstrap compiler; tests use the
# resulting current-generation binary, never the frozen Host compiler.
BOOT_BIN=$(bootstrap_stage "$ROOT" "$ROOT/.tmp/bootstrap-stage" "${STRICT_FLAGS[@]}")
cp "$BOOT_BIN" "$ROOT/.tmp/bootstrap.bin"
BOOT_BIN="$ROOT/.tmp/bootstrap.bin"

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

assert_source_mapping() {
  local c_file=$1 source_file=$2 source_line=$3 label=$4
  local directive="#line ${source_line} \"${source_file}\""
  if ! grep -Fqx -- "$directive" "$c_file"; then
    echo "FAIL $label: missing source mapping '$directive'" >&2
    return 1
  fi
}

compile_run() {
  local source=$1 label=$2
  track_source "$source"
  local boot_c="$OUT/${label}.boot.c"
  local boot_bin="$OUT/${label}.boot.bin"
  rm -f "$boot_c" "$boot_bin" "$ROOT"/$(basename "$source").c
  "$BOOT_BIN" "$source" "$boot_c" >/dev/null
  if [[ "$label" == "include_test_main" ]]; then
    assert_single_runtime_prologue "$boot_c" "$label (Bootstrap)"
    assert_source_mapping "$boot_c" "$ROOT/tests/regression/include_test_nested.bsl" 2 "$label (nested include)"
    assert_source_mapping "$boot_c" "$ROOT/tests/regression/include_test_lib.bsl" 3 "$label (include library)"
    assert_source_mapping "$boot_c" "$ROOT/tests/regression/include_test_main.bsl" 8 "$label (main source)"
  fi
  if [[ "$label" == "source_mapping_test" ]]; then
    assert_source_mapping "$boot_c" "$ROOT/tests/regression/source_mapping_test.bsl" 3 "$label"
  fi
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  "$boot_bin"
  printf 'PASS %s\n' "$label"
}

compile_run_with_input() {
  local source=$1 label=$2 input=$3 expected=$4
  track_source "$source"
  local boot_c="$OUT/${label}.boot.c"
  local boot_bin="$OUT/${label}.boot.bin"
  local stdout_file="$OUT/${label}.stdout"
  rm -f "$boot_c" "$boot_bin" "$stdout_file" "$ROOT"/$(basename "$source").c
  "$BOOT_BIN" "$source" "$boot_c" >/dev/null
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  printf '%s' "$input" | "$boot_bin" >"$stdout_file"
  diff -u <(printf '%s' "$expected") "$stdout_file"
  printf 'PASS %s (stdin/stdout)\n' "$label"
}

expect_runtime_failure_with_input() {
  local source=$1 label=$2 input=$3 expected_code=$4
  track_source "$source"
  local boot_c="$OUT/${label}.boot.c"
  local boot_bin="$OUT/${label}.boot.bin"
  rm -f "$boot_c" "$boot_bin" "$ROOT"/$(basename "$source").c
  "$BOOT_BIN" "$source" "$boot_c" >/dev/null
  gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$boot_c" -o "$boot_bin"
  set +e
  printf '%s' "$input" | "$boot_bin" >"$OUT/${label}.stdout" 2>"$OUT/${label}.stderr"
  local actual_code=$?
  set -e
  if [[ "$actual_code" -ne "$expected_code" ]]; then
    echo "FAIL $label: expected runtime exit $expected_code, got $actual_code" >&2
    return 1
  fi
  printf 'PASS %s (runtime rejected)\n' "$label"
}

expect_reject() {
  local source=$1 label=$2
  track_source "$source"
  local boot_log="$OUT/${label}.boot.log"
  rm -f "${source}.c" "$OUT/${label}.boot.c"
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
  grep -Fq 'distinct functions collide after C mangling' "$OUT/${label}.boot.log"
}

compile_run "$ROOT/tests/stress/modulo_stress.bsl" modulo_stress
compile_run "$ROOT/tests/regression/stdlib_growth_test.bsl" stdlib_growth_test
compile_run "$ROOT/tests/regression/stdlib_containers_test.bsl" stdlib_containers_test
compile_run "$ROOT/tests/regression/option_test.bsl" option_test
compile_run "$ROOT/tests/regression/option_result_combinators_test.bsl" option_result_combinators_test
compile_run "$ROOT/tests/regression/string_builder_iter_test.bsl" string_builder_iter_test
compile_run "$ROOT/tests/regression/numeric_compound_test.bsl" numeric_compound_test
compile_run "$ROOT/tests/regression/compound_assignment_side_effect_test.bsl" compound_assignment_side_effect_test
compile_run "$ROOT/tests/regression/source_mapping_test.bsl" source_mapping_test
compile_run "$ROOT/tests/regression/tagged_union_test.bsl" tagged_union_test
compile_run "$ROOT/tests/regression/concurrency_test.bsl" concurrency_test
compile_run "$ROOT/tests/regression/aligned_alloc_test.bsl" aligned_alloc_test
compile_run "$ROOT/tests/regression/fixed_width_integer_test.bsl" fixed_width_integer_test
expect_reject "$ROOT/tests/regression/fixed_width_invalid_generic.bsl" fixed_width_invalid_generic
expect_reject "$ROOT/tests/regression/aligned_invalid_power.bsl" aligned_invalid_power
expect_reject "$ROOT/tests/regression/concurrency_invalid_atomic.bsl" concurrency_invalid_atomic
expect_reject "$ROOT/tests/regression/concurrency_invalid_callback.bsl" concurrency_invalid_callback
expect_reject "$ROOT/tests/regression/tagged_union_arity_invalid.bsl" tagged_union_arity_invalid
expect_reject "$ROOT/tests/regression/tagged_union_type_invalid.bsl" tagged_union_type_invalid
compile_run_with_input "$ROOT/tests/regression/io_safe_test.bsl" io_safe_test $'42\nbad-number\nBasalt-OVERFLOW\nok\n' $'safe-io\n42\nBasalt-\n'
compile_run_with_input "$ROOT/tests/regression/io_safe_edge_test.bsl" io_safe_edge_test $'-17\n999999999999999999999999999999999999999999999\n' ''
expect_runtime_failure_with_input "$ROOT/tests/regression/io_invalid_limit.bsl" io_invalid_limit '' 2
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
printf 'Bootstrap-only regression checks completed successfully.\n'
