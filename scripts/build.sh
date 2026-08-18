#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT/src/compiler"
dune build bin/basaltc.exe
printf 'Host compiler: %s\n' "$ROOT/src/compiler/_build/default/bin/basaltc.exe"
"$ROOT/scripts/check_parser_conflicts.sh"
