# Complex Nested-Structure and Deep-Recursion Benchmark

## Scope

This benchmark compares Basalt-generated C with a hand-written C baseline for a nested data-processing workload. The workload contains a `Workspace` holding two `Matrix` values; each matrix owns two dynamically growing `array::Array<int>` containers. It initializes row metadata and cell data, performs checksum traversal, applies a recursive mixing function, and explicitly releases both matrices.

The benchmark is intentionally larger and structurally more demanding than the small comparison kernel in `benchmark_compare_2026`. It exercises ordinary structs containing generic container specializations, dynamic-array growth, by-value struct passing, nested cleanup, and recursion depths of 64, 128, and 256.

## Workloads

| Case | Rows | Columns | Cells per matrix | Recursion depth | Total cells |
| --- | ---: | ---: | ---: | ---: | ---: |
| Small | 5,000 | 24 | 120,000 | 64 | 240,000 |
| Medium | 20,000 | 32 | 640,000 | 128 | 1,280,000 |
| Large | 40,000 | 48 | 1,920,000 | 256 | 3,840,000 |

The arithmetic uses the same bounded integer formulas in both implementations. Every run first validates that the generated C and the hand-written C produce identical two-line checksum output.

## Measurement protocol

Both variants are compiled with GCC using `-std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. Each case uses 21 process executions per implementation, and the reported value is the median end-to-end process time measured with `time.perf_counter()`.

The Basalt source is translated using the current Bootstrap compiler. Generated C, binaries, compiler logs, variant sources, and TSV measurements are written under `.tmp/` and are not committed. Reproduce the benchmark from the repository root with:

```sh
source scripts/bootstrap_stage.sh
current_bin=$(bootstrap_stage "$PWD" .tmp/complex_nested_recursive_build \
  -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
python3 tests/benchmark/complex_nested_recursive/measure.py "$PWD" "$PWD/.tmp/complex_nested_recursive_build/bootstrap.current.bin"
```

## Reference run

The following reference run was performed after the recursive specialization and dependency-ordering changes. Timings are machine- and scheduler-dependent; the checksum equality and compile success are the invariant checks.

| Case | Basalt median | C median | Basalt delta | Basalt source | C source | Basalt binary | C binary |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Small | 7.883 ms | 6.724 ms | +17.231% | 2,358 B | 4,055 B | 16,952 B | 16,376 B |
| Medium | 45.943 ms | 40.982 ms | +12.105% | 2,361 B | 4,057 B | 16,952 B | 16,376 B |
| Large | 157.020 ms | 143.628 ms | +9.325% | 2,361 B | 4,057 B | 16,952 B | 16,376 B |

The generated C is within approximately 9–17% of this hand-written baseline in the measured end-to-end process runs. The relative gap narrows as the data workload grows, which is consistent with fixed runtime/startup and allocation-tracking costs being amortized over more cell traversal. The three cases scale approximately linearly with total cell count, while the deeper recursion adds a bounded, explicitly measured component.

This is not a claim of universal language performance. The C baseline uses the same array growth policy and formulas but does not include Basalt's allocation-tracking registry and runtime cleanup machinery. The benchmark therefore measures the practical generated-program path and makes the runtime-support cost visible.

## Generated C inspection: callback and borrow fixture

The generated C for `generic_callback_borrow_lifecycle_test.bsl` was compiled with the same strict GCC flags and executed successfully. The generic callback specializations are emitted as ordinary C function-pointer signatures:

```c
int apply__int(int value, int (*callback)(int));
int apply_borrowed__int(int *value, int (*callback)(int));
float apply__float(float value, float (*callback)(float));
```

The emitted program preserves the important source-level behavior. `&increment_int` and `&shift_f32` become function-pointer values, `apply_borrowed__int` dereferences the borrowed pointer exactly once into a local snapshot before invoking the callback, and the block-scoped pointer is no longer used after the block. `#line` directives point diagnostics and debugger locations back to the `.bsl` fixture.

The emitter also produces a harmless duplicate `return 0;` at the end of `main` after the explicit source return. GCC accepts it under the repository's strict warnings, but it remains a possible future code-quality cleanup because it adds unreachable generated text without changing behavior.

## Interpretation

The complex benchmark confirms two separate properties. First, recursive specialization and dependency-aware definition ordering emit complete C declarations for nested generic containers before the ordinary `Matrix` and `Workspace` definitions that contain them. Second, generated C remains operationally close to the matching C baseline while retaining the safety and cleanup runtime that makes the emitted source larger than a minimal hand-written kernel.
