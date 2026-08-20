# Bootstrap-only development

Basalt modern development is intentionally **Bootstrap-only**. The OCaml Host compiler under `src/compiler/` is frozen historical infrastructure and must not be modified, built, or used to compile new language or standard-library changes.

## Compiler lineage

The repository keeps two C artifacts with distinct roles:

| Artifact | Role | Policy |
|---|---|---|
| `src/bootstrap/basaltc.bsl.c` | Frozen C seed preserved from the previous development generation | Never overwrite it during modern development |
| `src/bootstrap/basaltc.modern.c` | Self-translated modern Bootstrap compiler | Use it as the compiler for current Basalt source and tests |

Every active runner compiles `basaltc.modern.c` directly with strict C11 GCC flags and uses the resulting binary for all modern work. The current `src/bootstrap/basaltc.bsl` is intentionally allowed to evolve beyond the stored compiler's source generation capabilities; the next compiler generation is produced only when the new source is ready to be translated by that stored compiler.

## Development loop

The standard loop is:

```bash
bash scripts/build.sh
bash scripts/fixed_point.sh
bash scripts/run_regression.sh
bash scripts/run_ownership_stress.sh
python3 scripts/run_memory_sanitizer.py
```

New language features must be implemented in `src/bootstrap/basaltc.bsl`, and new tests must be compiled and run through the Bootstrap compiler produced from `basaltc.modern.c`. Generated C files and binaries belong only in `.tmp/` and must not be committed.

## Fixed point

`scripts/fixed_point.sh` uses the stored modern compiler and performs the contemporary self-hosting loop:

1. `basaltc.modern.c` is compiled to `n1.bin`.
2. `n1.bin` translates the current `basaltc.bsl` to `n2.c`.
3. `n2.c` is compiled to `n2.bin`.
4. `n2.bin` translates the same current source to `n3.c`.
5. The runner requires `n2.c == n3.c` and checks the production SHA-256 value.

The stored C compiler is never regenerated or replaced by any runner. It is the deliberate compiler boundary between the previous generation and modern source development.

## Validation policy

The Bootstrap-only suites cover strict GCC compilation, valid and invalid regression fixtures, long-term conformance generation, ownership and move/borrow checks, pointer and function-pointer stress, adversarial sanitizer workloads, code-buffer growth, and fixed-point stability. The memory sanitizer harness compares Bootstrap-generated C with a hand-written C baseline; it does not invoke the frozen Host compiler.

The compatibility script `scripts/check_parser_conflicts.sh` is retained only as a legacy entry point. It intentionally performs no OCaml, Dune, Menhir, or Host-parser operation.
