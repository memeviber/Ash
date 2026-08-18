#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SRC="$ROOT/src/bootstrap/ashc.ash"
OUT="$ROOT/.tmp/fixed-point"
mkdir -p "$OUT"

(cd "$ROOT/src/compiler" && dune build bin/main.exe)
(cd "$ROOT/src/compiler" && "$ROOT/src/compiler/_build/default/bin/main.exe" "$SRC")
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$ROOT/src/bootstrap/ashc.ash.c" -o "$OUT/n1.bin"
"$OUT/n1.bin" "$SRC" "$OUT/n2.c"
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$OUT/n2.c" -o "$OUT/n2.bin"
"$OUT/n2.bin" "$SRC" "$OUT/n3.c"
cmp -s "$OUT/n2.c" "$OUT/n3.c"
sha256sum "$OUT/n2.c" | tee "$OUT/n2.sha256"
printf 'Fixed point verified: n2.c == n3.c\n'
