#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$ROOT/src/compiler"
dune build bin/main.exe
printf 'Host compiler: %s\n' "$ROOT/src/compiler/_build/default/bin/main.exe"
