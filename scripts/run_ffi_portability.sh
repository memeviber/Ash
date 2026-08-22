#!/usr/bin/env bash
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT="$ROOT/.tmp/ffi-portability"
STRICT=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
SOURCE="$ROOT/tests/regression/ffi_c_abi_matrix_valid.basalt"
NAMED="$ROOT/tests/regression/ffi_named_struct_valid.basalt"
mkdir -p "$OUT"
BOOT_BIN=$(source "$ROOT/scripts/bootstrap_stage.sh" && bootstrap_stage "$ROOT" "$OUT/bootstrap-stage" "${STRICT[@]}")

"$BOOT_BIN" "$SOURCE" "$OUT/ffi_c_abi_matrix.c" >/dev/null
"$BOOT_BIN" "$NAMED" "$OUT/ffi_named_struct.c" >/dev/null

gcc "${STRICT[@]}" "$OUT/ffi_c_abi_matrix.c" -o "$OUT/ffi_c_abi_matrix.gcc.bin"
"$OUT/ffi_c_abi_matrix.gcc.bin"
gcc "${STRICT[@]}" "$OUT/ffi_named_struct.c" -o "$OUT/ffi_named_struct.gcc.bin"
"$OUT/ffi_named_struct.gcc.bin"
printf '%s\n' 'PASS FFI GCC strict C11'

if command -v clang >/dev/null 2>&1; then
  clang "${STRICT[@]}" "$OUT/ffi_c_abi_matrix.c" -o "$OUT/ffi_c_abi_matrix.clang.bin"
  "$OUT/ffi_c_abi_matrix.clang.bin"
  clang "${STRICT[@]}" "$OUT/ffi_named_struct.c" -o "$OUT/ffi_named_struct.clang.bin"
  "$OUT/ffi_named_struct.clang.bin"
  printf '%s\n' 'PASS FFI Clang strict C11'
else
  printf '%s\n' 'SKIP FFI Clang: clang not installed'
fi

if command -v x86_64-w64-mingw32-gcc >/dev/null 2>&1; then
  x86_64-w64-mingw32-gcc "${STRICT[@]}" -c "$OUT/ffi_c_abi_matrix.c" -o "$OUT/ffi_c_abi_matrix.mingw.o"
  x86_64-w64-mingw32-gcc "${STRICT[@]}" -c "$OUT/ffi_named_struct.c" -o "$OUT/ffi_named_struct.mingw.o"
  printf '%s\n' 'PASS FFI MinGW strict C11 object checks'
else
  printf '%s\n' 'SKIP FFI MinGW: x86_64-w64-mingw32-gcc not installed'
fi
