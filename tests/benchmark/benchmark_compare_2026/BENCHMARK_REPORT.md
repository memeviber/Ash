# Benchmark Report

## Scope

This benchmark compares the generated C produced by Ash with hand-written C, Nim, and Cython implementations of the same small data-processing kernel. It is a reproducibility fixture rather than a universal language ranking. The repository contains the source programs, generated artifacts, timing helper, and measurement notes used by the experiment.

## Algorithm

For `n = 1,000,000`, each implementation allocates a data buffer, initializes each element with `(i * 17 + 23) mod 1009`, computes two checksums using modulus `1,000,003`, prints the checksums, and releases the buffer. The expected output is:

```text
268688
41439
```

Modulo reduction is performed during the loops to avoid signed-overflow ambiguity and to keep integer semantics comparable across implementations.

## Included artifacts

| File | Purpose |
| --- | --- |
| `kernel.ash` | Ash source implementation |
| `kernel.ash.c` | C emitted by Ash |
| `kernel.c` | Hand-written C baseline |
| `kernel.nim` | Nim implementation |
| `kernel_cython.c` | Cython-generated C artifact |
| `measure.py` | Measurement helper |

Compiler caches and machine-specific absolute paths are intentionally excluded from the public repository. Users should regenerate artifacts with local toolchains before drawing new performance conclusions.

## Toolchain and protocol

The original experiment used GCC 13.3.0 on Linux amd64. C and Ash were compiled with `-std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`. Nim used release optimization, while Cython used typed C-level loops together with its CPython module runtime. Each binary was run for nine samples, with ten executions per sample; the reported values are end-to-end process times rather than isolated in-process kernel times.

| Variant | Median time per run | Relative to hand-written C |
| --- | ---: | ---: |
| Hand-written C | 8.082 ms | 1.000x |
| Ash | 8.109 ms | 1.003x |
| Nim | 14.073 ms | 1.741x |
| Cython | 44.544 ms | 5.511x |

The four implementations produced the same checksum output. Ash's median was approximately 0.33% above the C baseline in this workload. Nim's version used `Seq[int]`, so its generated program carried sequence metadata, bounds checks, and Nim runtime support. Cython's end-to-end result included CPython module initialization and runtime startup.

## Generated-code observations

The hand-written C baseline is intentionally minimal: it performs direct allocation, two loops, checksum updates, printing, and `free`. Ash emits the same kernel control flow inline, but also includes a reusable runtime for allocation tracking, cleanup, include handling, and array helpers. This makes Ash-generated C longer than the hand-written baseline while keeping the generated kernel straightforward and preserving strict C compilation for the Ash path.

Nim's generated C contains sequence descriptors and safety checks because the source uses a high-level dynamic sequence. Cython's generated C is much larger because an embedded Python module must include CPython initialization, exception handling, reference management, and module machinery. These are meaningful engineering differences, but they are not evidence that one language is universally faster or smaller.

The benchmark therefore supports a narrow conclusion:

> **For this kernel, Ash produces correct C with runtime performance close to hand-written C, while its generated source remains larger because of the automatic runtime and allocation tracking.**

## Fairness and limitations

The benchmark measures process-level execution, so startup cost is included. This is useful for command-line experience but makes the Cython result sensitive to Python initialization. A separate in-process benchmark would be needed to isolate the loop itself. Results also depend on compiler versions, optimization flags, CPU model, operating-system scheduling, allocator behavior, and runtime library versions.

The comparison is best interpreted as a code-generation and runtime fixture. It demonstrates that Ash's generated C can be independently inspected, compiled, and compared with other C-emitting toolchains. It should not be used to claim that Ash, Nim, or Cython has a universal performance advantage.
