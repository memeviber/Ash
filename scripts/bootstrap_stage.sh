#!/usr/bin/env bash
# Shared two-stage Bootstrap builder.
# The stored C compiler is the only compiler seed. It translates the current
# Bootstrap source; all modern tests then use the resulting compiler binary.

bootstrap_stage() {
  local root=$1
  local out=$2
  shift 2
  local cflags=("$@")
  local seed_bin="$out/bootstrap.seed.bin"
  local current_c="$out/basaltc.current.c"
  local current_bin="$out/bootstrap.current.bin"

  mkdir -p "$out"
  gcc "${cflags[@]}" "$root/src/bootstrap/basaltc.seed.c" -o "$seed_bin"
  # Keep the helper's stdout machine-readable: compiler diagnostics belong on stderr.
  "$seed_bin" "$root/src/bootstrap/basaltc.basalt" "$current_c" >&2
  gcc "${cflags[@]}" "$current_c" -o "$current_bin"
  printf '%s\n' "$current_bin"
}
