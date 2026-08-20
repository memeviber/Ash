#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
MODERN_C="$ROOT/src/bootstrap/basaltc.modern.c"
OUT="$ROOT/.tmp/bootstrap-build"
mkdir -p "$OUT"

# The stored C compiler is the sole modern compiler; the Host is frozen.
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror "$MODERN_C" -o "$OUT/basaltc"
printf 'Bootstrap compiler: %s\n' "$OUT/basaltc"
