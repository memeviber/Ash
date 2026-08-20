#!/usr/bin/env bash
set -euo pipefail

# The OCaml Host compiler and its parser diagnostics are frozen. Modern
# development is intentionally Bootstrap-only; this helper is retained as a
# compatibility entry point and performs no Host build or parser inspection.
printf '%s\n' 'Host parser conflict check skipped: the Host compiler is frozen; use Bootstrap-only validation.'
