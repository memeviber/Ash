#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
DEFAULT_INPUT="$ROOT/src/bootstrap/basaltc.seed.c"
CHECK_ONLY=0
INPUT="$DEFAULT_INPUT"

usage() {
  cat >&2 <<'USAGE'
Usage: scripts/format_seed_c.sh [--check] [path]

Format a generated Bootstrap C seed with the repository .clang-format policy.
With --check, fail if the file is not already formatted.
The default path is src/bootstrap/basaltc.seed.c.
USAGE
}

while (($# > 0)); do
  case "$1" in
    --check)
      CHECK_ONLY=1
      shift
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --*)
      echo "Unknown option: $1" >&2
      usage
      exit 2
      ;;
    *)
      if [[ "$INPUT" != "$DEFAULT_INPUT" ]]; then
        echo "Only one input path may be supplied" >&2
        usage
        exit 2
      fi
      INPUT=$1
      shift
      ;;
  esac
done

if [[ ! -f "$INPUT" ]]; then
  echo "Seed C file not found: $INPUT" >&2
  exit 1
fi

if ! command -v clang-format >/dev/null 2>&1; then
  echo "clang-format is required to format Bootstrap C; install Clang-format 18 or newer" >&2
  exit 1
fi

if [[ "$CHECK_ONLY" -eq 1 ]]; then
  clang-format --style=file --dry-run --Werror "$INPUT"
  printf 'Seed C formatting check passed: %s\n' "$INPUT"
else
  clang-format --style=file -i "$INPUT"
  printf 'Seed C formatted: %s\n' "$INPUT"
fi
