#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DEFAULT_CANDIDATE="$ROOT/.tmp/fixed-point/n3.c"
SEED_C="$ROOT/src/bootstrap/basaltc.seed.c"
CHECKSUM_FILE="$ROOT/src/bootstrap/fixed_point_production.sha256"
CANDIDATE=${1:-$DEFAULT_CANDIDATE}

usage() {
  cat >&2 <<'USAGE'
Usage: scripts/promote_seed.sh [stable-candidate.c]

Promote a fixed-point C compiler candidate to the frozen Bootstrap seed,
format it with the repository Clang-format policy, and refresh the seed SHA-256.
The default candidate is .tmp/fixed-point/n3.c.
USAGE
}

if [[ "$CANDIDATE" == "--help" || "$CANDIDATE" == "-h" ]]; then
  usage
  exit 0
fi

if [[ "$CANDIDATE" == -* ]]; then
  echo "Unknown option: $CANDIDATE" >&2
  usage
  exit 2
fi

if [[ ! -f "$CANDIDATE" ]]; then
  echo "Stable seed candidate not found: $CANDIDATE" >&2
  echo "Run scripts/fixed_point.sh first, or pass an explicit candidate path." >&2
  exit 1
fi

if [[ "$CANDIDATE" == "$DEFAULT_CANDIDATE" ]]; then
  SIBLING_N4="$ROOT/.tmp/fixed-point/n4.c"
  if [[ ! -f "$SIBLING_N4" ]]; then
    echo "Missing fixed-point companion: $SIBLING_N4" >&2
    echo "Run scripts/fixed_point.sh first." >&2
    exit 1
  fi
  if ! cmp -s "$CANDIDATE" "$SIBLING_N4"; then
    echo "Refusing promotion: n3.c and n4.c are not identical" >&2
    exit 1
  fi
fi

if [[ "$CANDIDATE" == "$SEED_C" ]]; then
  echo "The candidate must be different from the destination seed path" >&2
  exit 2
fi

cp -- "$CANDIDATE" "$SEED_C"
"$ROOT/scripts/format_seed_c.sh" "$SEED_C"
printf '%s\n' "$(sha256sum "$SEED_C" | awk '{print $1}')" > "$CHECKSUM_FILE"
"$ROOT/scripts/format_seed_c.sh" --check "$SEED_C"
printf 'Seed promoted, formatted, and checksum synchronized: %s\n' "$SEED_C"
printf 'Seed SHA-256: %s\n' "$(cat "$CHECKSUM_FILE")"
