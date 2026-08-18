#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SRC="$ROOT/src/bootstrap/basaltc.bsl"
CHECKSUM_FILE="$ROOT/src/bootstrap/fixed_point_production.sha256"
OUT="$ROOT/.tmp/fixed-point"
mkdir -p "$OUT"

(cd "$ROOT/src/compiler" && dune build bin/basaltc.exe)
(cd "$ROOT/src/compiler" && "$ROOT/src/compiler/_build/default/bin/basaltc.exe" "$SRC")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$ROOT/src/bootstrap/basaltc.bsl.c" -o "$OUT/n1.bin"
"$OUT/n1.bin" "$SRC" "$OUT/n2.c"
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$OUT/n2.c" -o "$OUT/n2.bin"
"$OUT/n2.bin" "$SRC" "$OUT/n3.c"
cmp -s "$OUT/n2.c" "$OUT/n3.c"
ACTUAL=$(sha256sum "$OUT/n2.c" | cut -d' ' -f1)
EXPECTED=$(cut -d' ' -f1 "$CHECKSUM_FILE")
if [ "$ACTUAL" != "$EXPECTED" ]; then
  echo "Fixed-point checksum mismatch: expected $EXPECTED got $ACTUAL" >&2
  exit 1
fi
printf 'Fixed point verified: n2.c == n3.c, checksum %s matches production\n' "$ACTUAL"
