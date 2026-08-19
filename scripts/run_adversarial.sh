#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST="$ROOT/src/compiler/_build/default/bin/basaltc.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/basaltc.bsl"
OUT="$ROOT/.tmp/adversarial"
rm -rf "$OUT"
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
(cd "$ROOT/src/compiler" && "$HOST" "$BOOT_SOURCE")
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT/src/bootstrap/basaltc.bsl.c" -o "$OUT/bootstrap.bin"

for source in \
  "$ROOT/tests/regression/complex_fnptr_ptrarith.bsl" \
  "$ROOT/tests/regression/macro_pointer_complex.bsl" \
  "$ROOT/tests/regression/stress_memory_loop.bsl" \
  "$ROOT/tests/stress/modulo_stress.bsl"; do
  name=$(basename "$source" .bsl)
  track_source "$source"
  (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1
  cp "${source}.c" "$OUT/${name}.host.c"
  "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1
  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$OUT/${name}.host.c" -o "$OUT/${name}.host.bin"
  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$OUT/${name}.boot.c" -o "$OUT/${name}.boot.bin"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/${name}.host.bin" >"$OUT/${name}.host.out"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/${name}.boot.bin" >"$OUT/${name}.boot.out"
  cmp -s "$OUT/${name}.host.out" "$OUT/${name}.boot.out"
  ! grep -Eqi 'ERROR:|runtime error:|LeakSanitizer' "$OUT/${name}.host.out" "$OUT/${name}.boot.out"
done

# Bounds-safe runtime check: both compiler paths must return the same fallback
# value and reject an invalid write without sanitizer diagnostics.
OOB_SOURCE="$ROOT/tests/adversarial/memory_oob_test.bsl"
track_source "$OOB_SOURCE"
OOB_OUT="$OUT/memory_oob"
(
  cd "$(dirname "$OOB_SOURCE")"
  "$HOST" "$(basename "$OOB_SOURCE")"
)
cp "${OOB_SOURCE}.c" "$OOB_OUT.host.c"
"$OUT/bootstrap.bin" "$OOB_SOURCE" "$OOB_OUT.boot.c" >"$OOB_OUT.boot.compile.log" 2>&1
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$OOB_OUT.host.c" -o "$OOB_OUT.host.bin"
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-omit-frame-pointer "$OOB_OUT.boot.c" -o "$OOB_OUT.boot.bin"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OOB_OUT.host.bin" >"$OOB_OUT.host.log" 2>&1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OOB_OUT.boot.bin" >"$OOB_OUT.boot.log" 2>&1
cmp -s "$OOB_OUT.host.log" "$OOB_OUT.boot.log"
! grep -Eqi 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer' "$OOB_OUT.host.log" "$OOB_OUT.boot.log"

printf 'Adversarial sanitizer checks passed, including deterministic OOB rejection.\n'
