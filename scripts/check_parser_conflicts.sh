#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
LIB="$ROOT/src/compiler/lib"

EXPECTED_STATES=23
EXPECTED_CONFLICTS=45

output=$(cd "$LIB" && menhir --explain parser.mly 2>&1 || true)

states=$(printf '%s\n' "$output" | sed -nE 's/^Warning: ([0-9]+) states have shift\/reduce conflicts\.$/\1/p')
conflicts=$(printf '%s\n' "$output" | sed -nE 's/^Warning: ([0-9]+) shift\/reduce conflicts were arbitrarily resolved\.$/\1/p')

if [ -z "$states" ] || [ -z "$conflicts" ]; then
  printf 'FAIL: could not parse menhir conflict report\n' >&2
  exit 1
fi

if [ "$states" -ne "$EXPECTED_STATES" ] || [ "$conflicts" -ne "$EXPECTED_CONFLICTS" ]; then
  printf 'FAIL: parser conflict count changed (%s states, %s conflicts; expected %s states, %s conflicts)\n' \
    "$states" "$conflicts" "$EXPECTED_STATES" "$EXPECTED_CONFLICTS" >&2
  printf 'Regenerate src/compiler/lib/parser.conflicts (menhir --explain) and update docs/DESIGN.md if the new conflicts are intended.\n' >&2
  exit 1
fi

printf 'Parser conflicts stable: %s states, %s conflicts (all resolved by default shift, see docs/DESIGN.md).\n' "$states" "$conflicts"