#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SRC="$ROOT/src/bootstrap/basaltc.basalt"
SEED_C="$ROOT/src/bootstrap/basaltc.seed.c"
CHECKSUM_FILE="$ROOT/src/bootstrap/fixed_point_production.sha256"
OUT="$ROOT/.tmp/fixed-point"
mkdir -p "$OUT"

STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)

# The frozen previous-generation compiler translates the evolving source once.
gcc "${STRICT_FLAGS[@]}" "$SEED_C" -o "$OUT/n1.bin"
"$OUT/n1.bin" "$SRC" "$OUT/n2.c"

# The generated compiler is then iterated until the current source reaches a
# stable self-hosted artifact. n2 may legitimately differ from the old seed.
gcc "${STRICT_FLAGS[@]}" "$OUT/n2.c" -o "$OUT/n2.bin"
"$OUT/n2.bin" "$SRC" "$OUT/n3.c"
gcc "${STRICT_FLAGS[@]}" "$OUT/n3.c" -o "$OUT/n3.bin"
"$OUT/n3.bin" "$SRC" "$OUT/n4.c"
cmp -s "$OUT/n3.c" "$OUT/n4.c"

SEED_ACTUAL=$(sha256sum "$SEED_C" | cut -d' ' -f1)
SEED_EXPECTED=$(cut -d' ' -f1 "$CHECKSUM_FILE")
if [ "$SEED_ACTUAL" != "$SEED_EXPECTED" ]; then
  echo "Frozen Bootstrap seed checksum mismatch: expected $SEED_EXPECTED got $SEED_ACTUAL" >&2
  exit 1
fi
CURRENT=$(sha256sum "$OUT/n3.c" | cut -d' ' -f1)
printf 'Bootstrap fixed point verified: frozen seed -> n2 -> n3 -> n4, n3.c == n4.c\n'
printf 'Frozen seed checksum: %s; current stable compiler checksum: %s\n' "$SEED_ACTUAL" "$CURRENT"
