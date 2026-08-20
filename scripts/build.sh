#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
OUT="$ROOT/.tmp/bootstrap-build"
source "$ROOT/scripts/bootstrap_stage.sh"

STRICT_FLAGS=(-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
BOOT_BIN=$(bootstrap_stage "$ROOT" "$OUT" "${STRICT_FLAGS[@]}")
printf 'Bootstrap compiler (current generation): %s\n' "$BOOT_BIN"
