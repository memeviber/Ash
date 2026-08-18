# Basalt

Basalt is a small, self-hosting programming language and C compiler. The project contains two implementations of the same language pipeline: an OCaml host compiler used as the reference implementation and a Bootstrap compiler written in Basalt itself. Both implementations parse Basalt source, perform static type checking, and emit portable C11.

The repository is organized for reproducible compiler work rather than for generated build output. Source code lives under `src/`, tests under `tests/`, documentation under `docs/`, and repeatable development commands under `scripts/`.

## Highlights

Basalt supports integers, booleans, characters, strings, floating-point values, pointers, fixed and dynamic arrays, structs, enums, namespaces, generic types, function pointers, controlled C FFI through `extern`, `include`, and `includec`, ownership checks, scope-aware type checking, and structured standard-library components such as `map` and `result`.

Arithmetic operators include `+`, `-`, `*`, `/`, and the modulo operator `%`. Modulo has multiplicative precedence and is accepted by both the Host and Bootstrap compilers. For example:

```basalt
func main(): int {
  let residue: int = (17 + 8) % 5;
  return residue;
}
```

> **Implementation note:** Basalt emits C11 and validates generated programs with strict GCC diagnostics. Floating-point arithmetic follows the current compiler rules; `%` is intended for integer operands in portable generated C.

## Repository layout

| Path | Contents |
| --- | --- |
| `src/compiler/` | OCaml Host compiler, Dune metadata, lexer, parser, type checker, AST, and C emitter |
| `src/bootstrap/` | Canonical self-hosting Basalt compiler source, generated C bootstrap artefact, and fixed-point checksum |
| `src/stdlib/` | Generic containers and standard library modules |
| `tests/regression/` | Focused language and compiler regression programs |
| `tests/stress/` | The 164-case corpus plus the dedicated modulo stress and negative tests |
| `tests/conformance/` | Host/Bootstrap conformance programs and runner material |
| `tests/adversarial/` | Sanitizer-oriented and adversarial compiler tests |
| `tests/benchmark/` | Cross-language benchmark source material |
| `docs/` | Language specification, design notes, naming policy, and release notes |
| `scripts/` | Build, test, and fixed-point commands |

## Building the Host compiler

From the repository root, run:

```sh
./scripts/build.sh
```

The script invokes Dune in `src/compiler/` and produces the Host executable at `src/compiler/_build/default/bin/basaltc.exe`. The compiler accepts an Basalt source path. It writes the generated C file beside the source file, using the source filename with `.c` appended.

## Building and using the Bootstrap compiler

The canonical Bootstrap source is `src/bootstrap/basaltc.bsl`. The standard sequence is:

```sh
./scripts/fixed_point.sh
```

The script first uses the Host compiler to generate C for the Bootstrap compiler, compiles that C with strict GCC flags, and then runs two Bootstrap generations. A successful run proves that `n2.c` and `n3.c` are byte-identical.

The strict compiler profile is:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

## Testing

The main commands are:

```sh
./scripts/run_regression.sh
./scripts/run_conformance.sh
./scripts/run_adversarial.sh
./scripts/run_stress.sh
```

The modulo-specific checks compile and execute the same Basalt program through both compilers. They cover ordinary residues, zero and one, self-modulo, loop accumulation, precedence, nested expressions, a negative-value simulation, a generic `Result` context, and rejection of string operands. The complete stress corpus currently reports 164/164 passing cases before the dedicated modulo cases are added to the release repository.

## Language documentation

Start with [`docs/LANGUAGE_SPEC.md`](docs/LANGUAGE_SPEC.md) for syntax and semantics. [`docs/DESIGN.md`](docs/DESIGN.md) describes the Host/Bootstrap architecture, [`docs/NAMING.md`](docs/NAMING.md) defines repository naming conventions, and [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md) records the current release changes.

## Portability and safety

Generated code is checked with strict C11 warnings and is exercised with AddressSanitizer and UndefinedBehaviorSanitizer in the adversarial and stress workflows. Dynamic-array allocation, resizing, indexing, mutation, and release use checked runtime helpers with registry validation, overflow guards, lifetime checks, and deterministic failure on invalid bounds. The Host and Bootstrap compilers share this fail-closed policy; the OOB negative fixture must terminate with exit code `2` in both paths.

Basalt does not yet provide complete Rust-style static borrow checking. Raw pointer dereference, pointer arithmetic, `extern`, and `includec` remain explicitly low-level interoperability boundaries. Programs using those features should follow the ownership rules documented in [`docs/MEMORY_SAFETY_AUDIT.md`](docs/MEMORY_SAFETY_AUDIT.md) and should be tested with sanitizers. The repository intentionally excludes compiler build directories, generated test outputs, caches, and local binaries from version control.

## License

Basalt is distributed under the MIT License. See [`LICENSE`](LICENSE).
