# Basalt

Basalt is a small, self-hosting programming language and C compiler. The project contains two implementations of the same language pipeline: an OCaml host compiler used as the reference implementation and a Bootstrap compiler written in Basalt itself. Both implementations parse Basalt source, perform static type checking, and emit portable C11.

The repository is organized for reproducible compiler work rather than for generated build output. Source code lives under `src/`, tests under `tests/`, documentation under `docs/`, and repeatable development commands under `scripts/`.

## Highlights

Basalt supports integers, booleans, characters, strings, floating-point values, pointers, fixed and dynamic arrays, structs, enums, namespaces, generic types, function pointers, controlled C FFI through `extern`, `include`, and `includec`, ownership checks, scope-aware type checking, and a namespace-qualified standard library covering `array`, `slice`, `map`, `set`, `deque`, `iter`, `option`, `result`, strings, paths, filesystem, time, processes, concurrency, formatting, randomness, I/O, and structured system execution.

Safety is a first-class concern:

- **Compile-time bounds checks for fixed arrays.** Indexing `T[n]` with a constant outside the array is rejected before any C is emitted — including the `0 - 1` form of `-1`.
- **Move/borrow checking for dynamic arrays.** Values own their buffers; passing them moves them, releasing consumes them, and borrows (`&`) block mutation, moves, and release while live.
- **Null safety through `option`.** The `option::Option<T>` module (tag + payload) makes absence explicit and total — the only way to read the payload is `unwrap_or`, so there is no panic path.
- **Runtime fail-closed policy.** Tracked allocations are registry-checked; invalid bounds and double releases terminate deterministically with exit code `2`.

The Bootstrap compiler's built-in function table is data-driven (`bi_register(name, tag, flags)`), so adding a built-in is a two-line change across the two compilers instead of edits to several hardcoded hash lists.

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
| `src/stdlib/` | Generic containers, text/path APIs, OS boundaries, concurrency, formatting, randomness, and standard library modules |
| `tests/regression/` | Focused language and compiler regression programs |
| `tests/stress/` | The 164-case corpus plus the dedicated modulo stress and negative tests |
| `tests/conformance/` | Host/Bootstrap conformance programs and runner material |
| `tests/adversarial/` | Sanitizer-oriented and adversarial compiler tests |
| `tests/benchmark/` | Cross-language benchmark source material |
| `docs/` | Language specification, design notes, naming policy, and release notes |
| `scripts/` | Build, test, and fixed-point commands |

## Package manager

The repository includes a deterministic, repository-side package manager at `scripts/basalt_pkg.py`. It reads `Basalt.toml`, resolves SemVer requirements against a read-only registry, writes `Basalt.lock`, verifies SHA-256 archives, and materializes verified source under `.basalt/vendor/`. The tool is independent of the frozen OCaml Host compiler and does not add package-import syntax to the Bootstrap compiler.

| Concern | Initial implementation |
| --- | --- |
| Manifest | `Basalt.toml` with package metadata and dependency requirements |
| Reproducibility | `Basalt.lock` with exact versions, sources, edges, and checksums |
| Artifact storage | `$BASALT_HOME/cache`, content-addressed by SHA-256 |
| Source materialization | `.basalt/vendor/<name>/<version>/`, promoted atomically after validation |
| Offline operation | `fetch --offline` and `build --offline`, using only lockfile and cache |

For a local registry or CI fixture, use the global options before the subcommand:

```sh
python3 scripts/basalt_pkg.py --root . --registry .tmp/registry fetch
python3 scripts/basalt_pkg.py --root . --registry .tmp/registry update
python3 scripts/basalt_pkg.py --root . fetch --offline
python3 scripts/basalt_pkg.py --root . verify
```

The full contract, registry record format, lockfile invariants, archive safety policy, and current build boundary are documented in [`docs/PACKAGE_MANAGER.md`](docs/PACKAGE_MANAGER.md). Package archives are source input only: the initial tool never executes package-provided scripts, and native compiler package imports remain a later compatibility milestone. For a real build, `--compiler` selects the Bootstrap compiler and `--cc` selects the C compiler, for example `python3 scripts/basalt_pkg.py --root . build --compiler .tmp/bootstrap.bin --cc clang --compiler-arg=-std=c11 --compiler-arg=-Wall --compiler-arg=-Werror`.

## Building the Host compiler

From the repository root, run:

```sh
./scripts/build.sh
```

The script invokes Dune in `src/compiler/` and produces the Host executable at `src/compiler/_build/default/bin/basaltc.exe`. The compiler accepts an Basalt source path. It writes the generated C file beside the source file, using the source filename with `.c` appended.

## Building and using the Bootstrap compiler

The canonical Bootstrap source is `src/bootstrap/basaltc.basalt`. The standard sequence is:

```sh
./scripts/fixed_point.sh
```

The script first uses the Host compiler to generate C for the Bootstrap compiler, compiles that C with strict GCC flags, and then runs two Bootstrap generations. A successful run proves that `n2.c` and `n3.c` are byte-identical.

The strict compiler profile is:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

## Testing

The master verification command is:

```sh
./scripts/run_ownership_stress.sh
```

It builds the current Bootstrap compiler from the frozen C seed, runs the move/borrow and standard-library ownership fixtures under ASan/UBSan with leak detection, requires invalid ownership fixtures to be rejected, and then runs the regression, stress, adversarial, conformance, and fixed-point suites, plus a guard that no executable may be left under `tests/`. The stdlib fixtures cover file/path/string ownership, time/process/format/random boundaries, concurrency handles, and iterator/container lifecycles.

The individual suites can be run directly:

```sh
./scripts/run_regression.sh
./scripts/run_conformance.sh
./scripts/run_adversarial.sh
./scripts/run_stress.sh
./scripts/fixed_point.sh
```

The regression suite compiles and executes every registered fixture through the Bootstrap compiler with strict GCC: valid programs must compile and run; `expect_reject` fixtures must be rejected. Selected stdlib fixtures are also compiled with strict Clang and sanitizer builds. The corpus covers collection growth and hashing, iterator callbacks, stable sorting, UTF-8/string boundaries, path normalization, text filesystem errors, time validation, secure argv process handling, mutex/cancellation, typed formatting, deterministic PRNG behavior, and ownership cleanup.

## Guides

- [`docs/USER_GUIDE.md`](docs/USER_GUIDE.md) — how to build Basalt, write programs, and use the standard library, with verified examples
- [`docs/DEVELOPER_GUIDE.md`](docs/DEVELOPER_GUIDE.md) — how the two compilers cooperate, the verification machinery, and worked examples of adding built-ins, stdlib modules, compiler features, and package-manager fixtures
- [`docs/PACKAGE_MANAGER.md`](docs/PACKAGE_MANAGER.md) — manifest, SemVer, registry, lockfile, cache, vendor, security, and build-boundary contract

## Language documentation

Start with [`docs/LANGUAGE_SPEC.md`](docs/LANGUAGE_SPEC.md) for syntax and semantics. [`docs/DESIGN.md`](docs/DESIGN.md) describes the Host/Bootstrap architecture, [`docs/NAMING.md`](docs/NAMING.md) defines repository naming conventions, and [`docs/RELEASE_NOTES.md`](docs/RELEASE_NOTES.md) records the current release changes.

## Portability and safety

Generated code is checked with strict C11 warnings under GCC and Clang and is exercised with AddressSanitizer and UndefinedBehaviorSanitizer in the ownership, adversarial, and stress workflows. Dynamic-array allocation, resizing, indexing, mutation, and release use checked runtime helpers with registry validation, overflow guards, lifetime checks, and deterministic failure on invalid bounds. Filesystem, process, time, concurrency, and entropy APIs are explicit OS boundaries with `Result`/status checks; text file reads are not a binary buffer abstraction, process handles must be reaped before release, and unsupported Windows capabilities return documented errors rather than being emulated unsafely.

Basalt does not yet provide complete Rust-style static borrow checking. Raw pointer dereference, pointer arithmetic, `extern`, and `includec` remain explicitly low-level interoperability boundaries. Programs using those features should follow the ownership rules documented in [`docs/MEMORY_SAFETY_AUDIT.md`](docs/MEMORY_SAFETY_AUDIT.md) and should be tested with sanitizers. The repository intentionally excludes compiler build directories, generated test outputs, caches, and local binaries from version control.

## License

Basalt is distributed under the MIT License. See [`LICENSE`](LICENSE).
