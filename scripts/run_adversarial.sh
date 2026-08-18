#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
HOST="$ROOT/src/compiler/_build/default/bin/main.exe"
BOOT_SOURCE="$ROOT/src/bootstrap/ashc.ash"
OUT="$ROOT/.tmp/adversarial"
rm -rf "$OUT"
mkdir -p "$OUT"

(cd "$ROOT/src/compiler" && dune build bin/main.exe)
(cd "$ROOT/src/compiler" && "$HOST" "$BOOT_SOURCE")
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -fsanitize=address,undefined -fno-omit-frame-pointer "$ROOT/src/bootstrap/ashc.ash.c" -o "$OUT/bootstrap.bin"

for source in \
  "$ROOT/tests/regression/complex_fnptr_ptrarith.ash" \
  "$ROOT/tests/regression/macro_pointer_complex.ash" \
  "$ROOT/tests/regression/stress_memory_loop.ash" \
  "$ROOT/tests/stress/modulo_stress.ash"; do
  name=$(basename "$source" .ash)
  (cd "$(dirname "$source")" && "$HOST" "$(basename "$source")") >"$OUT/${name}.host.log" 2>&1
  cp "${source}.c" "$OUT/${name}.host.c"
  "$OUT/bootstrap.bin" "$source" "$OUT/${name}.boot.c" >"$OUT/${name}.boot.log" 2>&1
  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -fsanitize=address,undefined -fno-omit-frame-pointer "$OUT/${name}.host.c" -o "$OUT/${name}.host.bin"
  gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -fsanitize=address,undefined -fno-omit-frame-pointer "$OUT/${name}.boot.c" -o "$OUT/${name}.boot.bin"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/${name}.host.bin" >"$OUT/${name}.host.out"
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OUT/${name}.boot.bin" >"$OUT/${name}.boot.out"
  cmp -s "$OUT/${name}.host.out" "$OUT/${name}.boot.out"
  ! grep -Eqi 'ERROR:|runtime error:|LeakSanitizer' "$OUT/${name}.host.out" "$OUT/${name}.boot.out"
done

# Negative runtime check: both compiler paths must reject an out-of-bounds
# dynamic-array read deterministically, without sanitizer diagnostics.
OOB_SOURCE="$ROOT/tests/adversarial/memory_oob_test.ash"
OOB_OUT="$OUT/memory_oob"
(
  cd "$(dirname "$OOB_SOURCE")"
  "$HOST" "$(basename "$OOB_SOURCE")"
)
cp "${OOB_SOURCE}.c" "$OOB_OUT.host.c"
"$OUT/bootstrap.bin" "$OOB_SOURCE" "$OOB_OUT.boot.c" >"$OOB_OUT.boot.compile.log" 2>&1
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -fsanitize=address,undefined -fno-omit-frame-pointer "$OOB_OUT.host.c" -o "$OOB_OUT.host.bin"
gcc -std=c11 -O1 -g -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror=implicit-function-declaration -Werror=incompatible-pointer-types -Werror=int-conversion -fsanitize=address,undefined -fno-omit-frame-pointer "$OOB_OUT.boot.c" -o "$OOB_OUT.boot.bin"
set +e
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OOB_OUT.host.bin" >"$OOB_OUT.host.log" 2>&1
host_status=$?
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 "$OOB_OUT.boot.bin" >"$OOB_OUT.boot.log" 2>&1
boot_status=$?
set -e
[ "$host_status" -eq 2 ]
[ "$boot_status" -eq 2 ]
# The Host includes a short panic diagnostic; the Bootstrap runtime intentionally
# keeps this path message-free. Exit status is the parity contract here.
! grep -Eqi 'AddressSanitizer|UndefinedBehaviorSanitizer|runtime error:|LeakSanitizer' "$OOB_OUT.host.log" "$OOB_OUT.boot.log"

printf 'Adversarial sanitizer checks passed, including deterministic OOB rejection.\n'
