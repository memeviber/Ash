#!/usr/bin/env bash
# Shared Bootstrap builder.
# The frozen C compiler is only the seed.  Build one translated compiler, then
# let that compiler translate the Bootstrap source again so tests exercise the
# current self-hosted implementation (including generated runtime helpers).

bootstrap_stage() {
  local root=$1
  local out=$2
  shift 2
  local cflags=("$@")
  local seed_bin="$out/bootstrap.seed.bin"
  local stage2_c="$out/basaltc.stage2.c"
  local stage2_bin="$out/bootstrap.stage2.bin"
  local current_c="$out/basaltc.current.c"
  local current_bin="$out/bootstrap.current.bin"

  mkdir -p "$out"
  gcc "${cflags[@]}" "$root/src/bootstrap/basaltc.seed.c" -o "$seed_bin"
  # Keep the helper's stdout machine-readable: compiler diagnostics belong on stderr.
  "$seed_bin" --no-line "$root/src/bootstrap/basaltc.basalt" "$stage2_c" >&2
  gcc "${cflags[@]}" "$stage2_c" -o "$stage2_bin"
  "$stage2_bin" --no-line "$root/src/bootstrap/basaltc.basalt" "$current_c" >&2
  gcc "${cflags[@]}" "$current_c" -o "$current_bin"
  printf '%s\n' "$current_bin"
}
