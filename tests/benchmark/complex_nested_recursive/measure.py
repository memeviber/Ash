#!/usr/bin/env python3
import csv
import pathlib
import statistics
import subprocess
import sys
import time

ROOT = pathlib.Path(sys.argv[1]).resolve()
COMPILER = pathlib.Path(sys.argv[2]).resolve()
WORK = ROOT / ".tmp" / "complex-benchmark-results"
WORK.mkdir(parents=True, exist_ok=True)
BSL_TEMPLATE = ROOT / "tests" / "benchmark" / "complex_nested_recursive" / "kernel.basalt"
C_TEMPLATE = ROOT / "tests" / "benchmark" / "complex_nested_recursive" / "kernel.c"

FLAGS = [
    "-std=c11",
    "-O2",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wconversion",
    "-Wshadow",
    "-Werror",
]
CASES = [
    ("small", 5000, 24, 64),
    ("medium", 20000, 32, 128),
    ("large", 40000, 48, 256),
]


def run_checked(command, stdout=None):
    return subprocess.run(command, check=True, stdout=stdout, stderr=subprocess.PIPE, text=True)


def make_variant(name, rows, columns, depth):
    bsl = WORK / f"kernel_{name}.basalt"
    csrc = WORK / f"kernel_{name}.c"
    bsl_text = BSL_TEMPLATE.read_text()
    bsl_text = bsl_text.replace(
        'include "../../../src/stdlib/array.basalt"',
        'include "../../src/stdlib/array.basalt"',
    )
    bsl_text = bsl_text.replace(
        "workspace.primary = make_matrix(20000, 32, 11);",
        f"workspace.primary = make_matrix({rows}, {columns}, 11);",
    )
    bsl_text = bsl_text.replace(
        "workspace.secondary = make_matrix(20000, 32, 29);",
        f"workspace.secondary = make_matrix({rows}, {columns}, 29);",
    )
    bsl_text = bsl_text.replace("workspace.rounds = 128;", f"workspace.rounds = {depth};")
    c_text = C_TEMPLATE.read_text()
    c_text = c_text.replace("#define ROW_COUNT 20000", f"#define ROW_COUNT {rows}")
    c_text = c_text.replace("#define COLUMN_COUNT 32", f"#define COLUMN_COUNT {columns}")
    c_text = c_text.replace("#define RECURSION_DEPTH 128", f"#define RECURSION_DEPTH {depth}")
    bsl.write_text(bsl_text)
    csrc.write_text(c_text)
    return bsl, csrc


def timed_runs(executable, expected):
    values = []
    for _batch in range(7):
        for _repeat in range(3):
            started = time.perf_counter()
            result = subprocess.run([str(executable)], check=True, capture_output=True, text=True)
            elapsed_ms = (time.perf_counter() - started) * 1000.0
            if result.stdout != expected:
                raise RuntimeError(f"checksum mismatch from {executable}: {result.stdout!r} != {expected!r}")
            values.append(elapsed_ms)
    return values


def main():
    output_tsv = WORK / "complex_benchmark.tsv"
    with output_tsv.open("w", newline="") as output:
        writer = csv.writer(output, delimiter="\t")
        writer.writerow([
            "case", "rows", "columns", "depth", "cells_per_matrix", "runs",
            "basalt_median_ms", "c_median_ms", "delta_percent",
            "basalt_source_bytes", "c_source_bytes", "basalt_binary_bytes", "c_binary_bytes",
        ])
        for name, rows, columns, depth in CASES:
            bsl, csrc = make_variant(name, rows, columns, depth)
            generated = bsl.with_suffix(".basalt.c")
            basalt_bin = WORK / f"kernel_{name}_basalt.bin"
            c_bin = WORK / f"kernel_{name}_c.bin"
            compiler_log = WORK / f"kernel_{name}.compiler.log"
            with compiler_log.open("w") as log:
                run_checked([str(COMPILER), str(bsl), str(generated)], stdout=log)
            run_checked(["gcc", *FLAGS, str(generated), "-o", str(basalt_bin)])
            run_checked(["gcc", *FLAGS, str(csrc), "-o", str(c_bin)])
            expected = subprocess.run([str(c_bin)], check=True, capture_output=True, text=True).stdout
            basalt_times = timed_runs(basalt_bin, expected)
            c_times = timed_runs(c_bin, expected)
            basalt_median = statistics.median(basalt_times)
            c_median = statistics.median(c_times)
            delta = ((basalt_median / c_median) - 1.0) * 100.0
            writer.writerow([
                name,
                rows,
                columns,
                depth,
                rows * columns,
                len(basalt_times),
                f"{basalt_median:.3f}",
                f"{c_median:.3f}",
                f"{delta:.3f}",
                bsl.stat().st_size,
                csrc.stat().st_size,
                basalt_bin.stat().st_size,
                c_bin.stat().st_size,
            ])
            output.flush()
    print(output_tsv)


if __name__ == "__main__":
    main()
