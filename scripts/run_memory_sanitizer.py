#!/usr/bin/env python3
"""Run repeatable ASan/LSan/UBSan ownership checks for Bootstrap Basalt and C.

The harness deliberately keeps the generated fixtures in .tmp so no binaries or
large generated C files enter the repository. The frozen Bootstrap C seed creates
the current compiler, which is checked against a hand-written C baseline with the
same observable result.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


BASALT_SOURCE = r'''include "../../../src/stdlib/array.basalt"
include "../../../src/stdlib/slice.basalt"
include "../../../src/stdlib/map.basalt"
include "../../../src/stdlib/string.basalt"

func hash_int(value: int): int {
  return value % 17;
}

func eq_int(left: int, right: int): bool {
  return left == right;
}

func main(): int {
  let round: int = 0;
  let checksum: int = 0;
  while round < 80 {
    let ints: array::Array<int> = array::new(1, 0);
    let chars: array::Array<char> = array::new(1, 'x');
    let values: slice::Slice<double> = slice::new(0.0);
    let table: map::HashMap<int, double> = map::new_with_hasher(0, 0.0, &hash_int, &eq_int);
    let text: string = "seed";
    let i: int = 0;

    while i < 48 {
      ints = array::push(ints, round + i, 0);
      chars = array::push(chars, 'a', 'x');
      values = slice::push(values, 0.5);
      table = map::put(table, round * 100 + i, 1.5);
      text = str::concat(text, "-x");
      i = i + 1;
    }

    let middle: array::Array<int> = array::slice(ints, 4, 20, 0);
    let value_count: int = slice::length(values);
    values = slice::clear(values);
    let j: int = 0;
    while j < 48 {
      if (j % 3) == 0 then {
        table = map::remove(table, round * 100 + j);
      }
      j = j + 1;
    }

    if array::length(ints) != 48 then return 10;
    if array::length(chars) != 48 then return 11;
    if array::length(middle) != 20 then return 12;
    if slice::length(values) != 0 then return 13;
    if map::contains_key(table, round * 100 + 1) == false then return 14;
    if str::byte_len(text) != 100 then return 15;

    checksum = checksum + array::get(ints, 47);
    checksum = checksum + 97;
    checksum = checksum + (value_count * 2);
    checksum = checksum + (map::length(table) * 3);
    checksum = checksum + str::byte_len(text);

    ints = array::free(ints);
    chars = array::free(chars);
    middle = array::free(middle);
    values = slice::free(values);
    table = map::free(table);
    round = round + 1;
  }
  print checksum;
  return 0;
}
'''

C_SOURCE = r'''#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *checked_realloc(void *ptr, size_t count, size_t size) {
  if (size != 0U && count > (size_t)-1 / size) {
    fputs("size overflow\n", stderr);
    exit(2);
  }
  void *next = realloc(ptr, count * size);
  if (next == NULL && count != 0U) {
    fputs("out of memory\n", stderr);
    exit(2);
  }
  return next;
}

static void append_text(char **text, size_t *length, size_t *capacity) {
  const char *suffix = "-x";
  const size_t suffix_len = 2U;
  const size_t needed = *length + suffix_len + 1U;
  if (needed > *capacity) {
    size_t next = *capacity == 0U ? 8U : *capacity;
    while (next < needed) next *= 2U;
    *text = checked_realloc(*text, next, sizeof(char));
    *capacity = next;
  }
  memcpy(*text + *length, suffix, suffix_len + 1U);
  *length += suffix_len;
}

static size_t slot_for(int key, size_t capacity) {
  unsigned int mixed = (unsigned int)key * 2654435761U;
  return (size_t)mixed % capacity;
}

static void map_insert(int *keys, double *values, unsigned char *used,
                       size_t capacity, int key, double value) {
  size_t slot = slot_for(key, capacity);
  while (used[slot] != 0U && keys[slot] != key) slot = (slot + 1U) % capacity;
  if (used[slot] == 0U) used[slot] = 1U;
  keys[slot] = key;
  values[slot] = value;
}

static bool map_contains(const int *keys, const unsigned char *used,
                         size_t capacity, int key) {
  size_t slot = slot_for(key, capacity);
  size_t probes = 0U;
  while (probes < capacity && used[slot] != 0U) {
    if (keys[slot] == key) return true;
    slot = (slot + 1U) % capacity;
    probes += 1U;
  }
  return false;
}

static void map_remove(int *keys, unsigned char *used, size_t capacity, int key) {
  size_t slot = slot_for(key, capacity);
  size_t probes = 0U;
  while (probes < capacity && used[slot] != 0U) {
    if (keys[slot] == key) {
      used[slot] = 0U;
      return;
    }
    slot = (slot + 1U) % capacity;
    probes += 1U;
  }
}

int main(void) {
  int checksum = 0;
  for (int round = 0; round < 80; ++round) {
    size_t int_len = 0U;
    size_t char_len = 0U;
    size_t value_len = 0U;
    size_t int_cap = 1U;
    size_t char_cap = 1U;
    size_t value_cap = 1U;
    int *ints = checked_realloc(NULL, int_cap, sizeof(int));
    char *chars = checked_realloc(NULL, char_cap, sizeof(char));
    double *values = checked_realloc(NULL, value_cap, sizeof(double));
    char *text = checked_realloc(NULL, 8U, sizeof(char));
    size_t text_len = 4U;
    size_t text_cap = 8U;
    memcpy(text, "seed", 5U);

    const size_t map_cap = 128U;
    int *keys = checked_realloc(NULL, map_cap, sizeof(int));
    double *map_values = checked_realloc(NULL, map_cap, sizeof(double));
    unsigned char *used = checked_realloc(NULL, map_cap, sizeof(unsigned char));
    memset(used, 0, map_cap * sizeof(unsigned char));
    size_t map_len = 0U;

    for (int i = 0; i < 48; ++i) {
      if (int_len == int_cap) {
        int_cap *= 2U;
        ints = checked_realloc(ints, int_cap, sizeof(int));
      }
      if (char_len == char_cap) {
        char_cap *= 2U;
        chars = checked_realloc(chars, char_cap, sizeof(char));
      }
      if (value_len == value_cap) {
        value_cap *= 2U;
        values = checked_realloc(values, value_cap, sizeof(double));
      }
      ints[int_len++] = round + i;
      chars[char_len++] = 'a';
      values[value_len++] = 0.5;
      const int key = round * 100 + i;
      if (!map_contains(keys, used, map_cap, key)) map_len += 1U;
      map_insert(keys, map_values, used, map_cap, key, 1.5);
      append_text(&text, &text_len, &text_cap);
    }

    int *middle = checked_realloc(NULL, 20U, sizeof(int));
    memcpy(middle, ints + 4, 20U * sizeof(int));
    const int value_count = (int)value_len;
    free(values);
    values = NULL;
    value_len = 0U;

    for (int j = 0; j < 48; ++j) {
      if ((j % 3) == 0) {
        const int key = round * 100 + j;
        if (map_contains(keys, used, map_cap, key)) {
          map_remove(keys, used, map_cap, key);
          map_len -= 1U;
        }
      }
    }

    if (int_len != 48U || char_len != 48U || map_len != 32U || text_len != 100U ||
        !map_contains(keys, used, map_cap, round * 100 + 1)) return 10;
    checksum += ints[47];
    checksum += 97;
    checksum += value_count * 2;
    checksum += (int)(map_len * 3U);
    checksum += (int)text_len;

    free(ints);
    free(chars);
    free(middle);
    free(values);
    free(keys);
    free(map_values);
    free(used);
    free(text);
  }
  printf("%d\n", checksum);
  return 0;
}
'''

SAN_FLAGS = [
    "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Wpedantic",
    "-Wconversion", "-Wshadow", "-Werror=implicit-function-declaration",
    "-Werror=incompatible-pointer-types", "-Werror=int-conversion",
    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
]
STRICT_FLAGS = [
    "-std=c11", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion",
    "-Wshadow", "-Werror",
]
DIAGNOSTIC_RE = re.compile(
    r"AddressSanitizer|LeakSanitizer|UndefinedBehaviorSanitizer|runtime error:|ERROR:|SUMMARY:"
)


def run(cmd: list[str], cwd: Path | None = None, env: dict[str, str] | None = None,
        stdout: Path | None = None, stderr: Path | None = None) -> subprocess.CompletedProcess[str]:
    out_handle = stdout.open("w", encoding="utf-8") if stdout else subprocess.PIPE
    err_handle = stderr.open("w", encoding="utf-8") if stderr else subprocess.PIPE
    try:
        return subprocess.run(cmd, cwd=cwd, env=env, text=True, stdout=out_handle,
                              stderr=err_handle, check=False)
    finally:
        if stdout:
            out_handle.close()
        if stderr:
            err_handle.close()


def text_of(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace") if path.exists() else ""


def compile_basalt(compiler: Path, source: Path, cwd: Path, log_dir: Path, label: str) -> Path:
    stdout = log_dir / f"{label}.compile.out"
    stderr = log_dir / f"{label}.compile.err"
    result = run([str(compiler), str(source), str(cwd / f"{source.stem}.{label}.c")], cwd=cwd, stdout=stdout, stderr=stderr)
    if result.returncode != 0:
        raise RuntimeError(f"Bootstrap compilation failed for {label}; see {stderr}")
    generated = cwd / f"{source.stem}.{label}.c"
    if not generated.exists():
        raise RuntimeError(f"Basalt did not produce {generated}")
    return generated


def run_sanitized(binary: Path, log_dir: Path, label: str) -> dict[str, object]:
    stdout = log_dir / f"{label}.run.out"
    stderr = log_dir / f"{label}.run.err"
    env = os.environ.copy()
    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1:abort_on_error=1"
    env["LSAN_OPTIONS"] = "detect_leaks=1:exitcode=23:report_objects=1"
    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
    result = run([str(binary)], env=env, stdout=stdout, stderr=stderr)
    err_text = text_of(stderr)
    return {
        "returncode": result.returncode,
        "stdout": text_of(stdout),
        "stderr": err_text,
        "sanitizer_diagnostic": bool(DIAGNOSTIC_RE.search(err_text)),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--keep", action="store_true", help="keep temporary generated artifacts")
    args = parser.parse_args()
    root = args.root.resolve()
    out = root / ".tmp" / "memory_sanitizer"
    if out.exists():
        shutil.rmtree(out)
    sources = out / "sources"
    logs = out / "logs"
    binaries = out / "bin"
    sources.mkdir(parents=True)
    logs.mkdir()
    binaries.mkdir()

    source = sources / "complex_ownership.basalt"
    c_source = sources / "complex_ownership.c"
    source.write_text(BASALT_SOURCE, encoding="utf-8")
    c_source.write_text(C_SOURCE, encoding="utf-8")

    seed_source = root / "src" / "bootstrap" / "basaltc.seed.c"
    seed_compiler = binaries / "bootstrap.seed.bin"
    current_source = sources / "basaltc.current.c"
    bootstrap_compiler = binaries / "bootstrap.current.bin"
    seed_build = run(["gcc", *STRICT_FLAGS, str(seed_source), "-o", str(seed_compiler)],
                     stdout=logs / "bootstrap-seed-build.out", stderr=logs / "bootstrap-seed-build.err")
    if seed_build.returncode != 0:
        raise RuntimeError(f"Bootstrap seed C build failed; see {logs / 'bootstrap-seed-build.err'}")
    current_build = run([str(seed_compiler), str(root / "src" / "bootstrap" / "basaltc.basalt"), str(current_source)],
                        stdout=logs / "bootstrap-current-generate.out", stderr=logs / "bootstrap-current-generate.err")
    if current_build.returncode != 0:
        raise RuntimeError(f"Current Bootstrap generation failed; see {logs / 'bootstrap-current-generate.err'}")
    current_compile = run(["gcc", *STRICT_FLAGS, str(current_source), "-o", str(bootstrap_compiler)],
                          stdout=logs / "bootstrap-current-build.out", stderr=logs / "bootstrap-current-build.err")
    if current_compile.returncode != 0:
        raise RuntimeError(f"Current Bootstrap C build failed; see {logs / 'bootstrap-current-build.err'}")
    bootstrap_generated = compile_basalt(bootstrap_compiler, source, sources, logs, "bootstrap")

    artifacts = {
        "bootstrap": bootstrap_generated,
        "c": c_source,
    }
    compile_results: dict[str, dict[str, object]] = {}
    for label, artifact in artifacts.items():
        binary = binaries / f"{label}.san.bin"
        result = run(["gcc", *SAN_FLAGS, str(artifact), "-o", str(binary)],
                     stdout=logs / f"{label}.build.out", stderr=logs / f"{label}.build.err")
        compile_results[label] = {
            "returncode": result.returncode,
            "stderr": text_of(logs / f"{label}.build.err"),
        }
        if result.returncode != 0:
            raise RuntimeError(f"Sanitizer build failed for {label}; see {logs / f'{label}.build.err'}")

    runs = {label: run_sanitized(binaries / f"{label}.san.bin", logs, label)
            for label in artifacts}
    outputs = {label: str(data["stdout"]).strip() for label, data in runs.items()}
    output_parity = len(set(outputs.values())) == 1
    sanitizer_clean = all(not bool(data["sanitizer_diagnostic"]) and data["returncode"] == 0
                          for data in runs.values())

    result_data = {
        "workload": "complex_ownership",
        "source": str(source.relative_to(root)),
        "artifacts": {label: str(path.relative_to(root)) for label, path in artifacts.items()},
        "compile": compile_results,
        "runs": runs,
        "output_parity": output_parity,
        "sanitizer_clean": sanitizer_clean,
        "expected_output": outputs["c"],
    }
    (out / "results.json").write_text(json.dumps(result_data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

    report = f"""# Basalt memory sanitizer report

## Result

The complex ownership workload was compiled and run through the frozen Bootstrap compiler and a hand-written C baseline under AddressSanitizer, LeakSanitizer leak detection, and UndefinedBehaviorSanitizer. Output parity: **{'PASS' if output_parity else 'FAIL'}**. Sanitizer status: **{'PASS' if sanitizer_clean else 'FAIL'}**.

| Variant | Return code | Output | Sanitizer diagnostics |
|---|---:|---|---|
"""
    for label, data in runs.items():
        report += f"| `{label}` | {data['returncode']} | `{str(data['stdout']).strip()}` | {'yes' if data['sanitizer_diagnostic'] else 'no'} |\n"
    report += """
## Coverage

The workload repeatedly creates and consumes an auto-growing `array::Array<int>`, an auto-growing `array::Array<char>`, a `slice::Slice<double>`, and a callback-backed `map::HashMap<int,double>`. It exercises growth, indexed reads, array slicing, slice clearing, map insertion, lookup, tombstone removal, string concatenation, and consuming-style `free` operations across 80 rounds. The C baseline performs the same observable operations with checked `realloc`, open-addressed storage, explicit text growth, and explicit `free` calls.

A clean run means that no AddressSanitizer, LeakSanitizer, or UndefinedBehaviorSanitizer diagnostic was emitted for these paths. It is a regression guard, not a proof of absence for every possible program or allocator failure path.

## Reproduction

```bash
python3 scripts/run_memory_sanitizer.py
```

Raw results and logs are under `.tmp/memory_sanitizer/`. The generated binaries and C files are temporary and are not intended for version control.
"""
    (out / "MEMORY_SANITIZER_REPORT.md").write_text(report, encoding="utf-8")

    print(f"output parity: {'PASS' if output_parity else 'FAIL'}")
    print(f"sanitizer clean: {'PASS' if sanitizer_clean else 'FAIL'}")
    for label, data in runs.items():
        print(f"{label}: rc={data['returncode']} output={str(data['stdout']).strip()!r} diagnostics={data['sanitizer_diagnostic']}")
    print(f"artifacts: {out}")
    return 0 if output_parity and sanitizer_clean else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
