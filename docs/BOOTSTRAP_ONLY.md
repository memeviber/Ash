# Bootstrap-only development

Basalt modern development is intentionally **Bootstrap-only**. The OCaml Host compiler under `src/compiler/` is frozen historical infrastructure and must not be modified, built, or used to compile new language or standard-library changes.

## Compiler lineage

The repository keeps two C artifacts with distinct roles:

| Artifact | Role | Policy |
|---|---|---|
| `src/bootstrap/basaltc.bsl.c` | Historical C artifact preserved from the previous development generation | Never overwrite it during modern development |
| `src/bootstrap/basaltc.seed.c` | Immutable C compiler seed used as the previous-generation boundary | Compile it only to translate the current Bootstrap source |
| `.tmp/*/basaltc.current.c` | Current-generation C compiler generated from `basaltc.bsl` | Temporary; compile it to produce the compiler used by modern tests |

Every active runner performs two stages. First, strict C11 GCC compiles `basaltc.seed.c` into a seed binary. Second, that seed binary translates the current `src/bootstrap/basaltc.bsl` into a current-generation C compiler, which is then compiled and used for all new language, standard-library, and fixture work. The current Bootstrap source is therefore free to evolve; it is not required to match the stored seed's source text.

## Development loop

The standard loop is:

```bash
bash scripts/build.sh
bash scripts/fixed_point.sh
bash scripts/run_regression.sh
bash scripts/run_ownership_stress.sh
python3 scripts/run_memory_sanitizer.py
```

New language features must be implemented in `src/bootstrap/basaltc.bsl`, and new tests must be compiled and run through the Bootstrap compiler produced from `basaltc.seed.c`. Generated C files and binaries belong only in `.tmp/` and must not be committed.

## Fixed point

`scripts/fixed_point.sh` uses the stored modern compiler and performs the contemporary self-hosting loop:

1. The frozen `basaltc.seed.c` is compiled to a seed binary.
2. The seed translates the current `basaltc.bsl` to `n2.c`; this may differ from the frozen seed because the source is allowed to evolve.
3. `n2.c` is compiled to `n2.bin`, which translates the source to `n3.c`.
4. `n3.c` is compiled to `n3.bin`, which translates the source to `n4.c`.
5. The runner requires `n3.c == n4.c`, checks the immutable seed SHA-256 value, and reports the current stable compiler SHA-256 separately.

The stored C compiler is never regenerated or replaced by any runner. It is the deliberate compiler boundary between the previous generation and modern source development; generated current-generation compilers remain temporary unless explicitly promoted as a future seed.

## Validation policy

The Bootstrap-only suites cover strict GCC compilation, valid and invalid regression fixtures, long-term conformance generation, ownership and move/borrow checks, pointer and function-pointer stress, adversarial sanitizer workloads, code-buffer growth, and fixed-point stability. The memory sanitizer harness compares Bootstrap-generated C with a hand-written C baseline; it does not invoke the frozen Host compiler.

The compatibility script `scripts/check_parser_conflicts.sh` is retained only as a legacy entry point. It intentionally performs no OCaml, Dune, Menhir, or Host-parser operation.
