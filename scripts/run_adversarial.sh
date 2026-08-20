#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
MODERN_C="$ROOT/src/bootstrap/basaltc.modern.c"
OUT="$ROOT/.tmp/adversarial"
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

# Bootstrap is the sole compiler under test. Use the stored modern C compiler
# directly and never invoke the frozen Host compiler.
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$MODERN_C" -o "$OUT/bootstrap.bin"

for source in \
  "$ROOT/tests/regression/complex_fnptr_ptrarith.bsl" \
  "$ROOT/tests/regression/macro_pointer_complex.bsl" \
  "$ROOT/tests/regression/stress_memory_loop.bsl" \
  "$ROOT/tests/stress/modulo_stress.bsl"; do
  name=$(basename "$source" .bsl)
  track_source "$source"
  "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1
  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$OUT/${name}.boot.c" -o "$OUT/${name}.boot.bin"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/${name}.boot.bin" >"$OUT/${name}.boot.out" 2>"$OUT/${name}.boot.err"
  ! grep -Eqi 'ERROR:|runtime error:|LeakSanitizer|AddressSanitizer|UndefinedBehaviorSanitizer' "$OUT/${name}.boot.err"
done

# Bounds-safe runtime check: the Bootstrap path must produce the deterministic
# fallback result without sanitizer diagnostics.
OOB_SOURCE="$ROOT/tests/adversarial/memory_oob_test.bsl"
track_source "$OOB_SOURCE"
OOB_OUT="$OUT/memory_oob"
"$OUT/bootstrap.bin" "$OOB_SOURCE" "$OOB_OUT.boot.c" >"$OOB_OUT.boot.compile.log" 2>&1
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$OOB_OUT.boot.c" -o "$OOB_OUT.boot.bin"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OOB_OUT.boot.bin" >"$OOB_OUT.boot.log" 2>&1
! grep -Eqi 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer' "$OOB_OUT.boot.log"

printf 'Bootstrap-only adversarial sanitizer checks passed, including deterministic OOB rejection.\n'
