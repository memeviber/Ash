# Bootstrap-only development

Basalt modern development is intentionally **Bootstrap-only**. The OCaml Host compiler under `src/compiler/` is frozen historical infrastructure and must not be modified, built, or used to compile new language or standard-library changes.

## Compiler lineage

The repository keeps two C artifacts with distinct roles:

| Artifact | Role | Policy |
|---|---|---|
| `src/bootstrap/basaltc.bsl.c` | Historical C artifact preserved from the previous development generation | Never overwrite it during modern development |
| `src/bootstrap/basaltc.seed.c` | Immutable C compiler seed used as the previous-generation boundary | Compile it only to translate the current Bootstrap source |
| `.tmp/*/basaltc.current.c` | Current-generation C compiler generated from `basaltc.basalt` | Temporary; compile it to produce the compiler used by modern tests |

Every active runner uses the current self-hosted generation rather than a stale one-stage translation. First, strict C11 GCC compiles `basaltc.seed.c` into a seed binary. The shared `scripts/bootstrap_stage.sh` then uses that seed to produce and compile a stage-2 compiler, uses stage 2 to translate the current `src/bootstrap/basaltc.basalt` into the current-generation C compiler (n3), and uses that current compiler for new language, standard-library, FFI, and fixture work. The current Bootstrap source is therefore free to evolve; it is not required to match the stored seed's source text. The generated C is additionally checked with Clang using the same strict warning profile; this catches C11 portability diagnostics such as `-Wstrict-prototypes`, `-Wparentheses-equality`, `-Wself-assign`, and `-Wunused-value` that GCC may not diagnose identically.

## Development loop

The standard loop is:

```bash
bash scripts/build.sh
bash scripts/fixed_point.sh
bash scripts/run_regression.sh
bash scripts/run_ownership_stress.sh
python3 scripts/run_memory_sanitizer.py
```

New language features must be implemented in `src/bootstrap/basaltc.basalt`, and new tests must be compiled and run through the Bootstrap compiler produced from `basaltc.seed.c`. Generated C files and binaries belong only in `.tmp/` and must not be committed. The compiler's `--compile` mode follows the same rule for its intermediate C file and executable during development.

## Fixed point

`scripts/fixed_point.sh` uses the stored modern compiler and performs the contemporary self-hosting loop:

1. The frozen `basaltc.seed.c` is compiled to a seed binary.
2. The seed translates the current `basaltc.basalt` to `n2.c` with `--no-line`; this may differ from the frozen seed because the source is allowed to evolve.
3. `n2.c` is compiled to `n2.bin`, which translates the source to `n3.c` with `--no-line`.
4. `n3.c` is compiled to `n3.bin`, which translates the source to `n4.c` with `--no-line`.
5. The runner requires `n3.c == n4.c`, checks the immutable seed SHA-256 value, and reports the current stable compiler SHA-256 separately.

The stored C compiler is never regenerated or replaced by any runner. It is the deliberate compiler boundary between the previous generation and modern source development; generated current-generation compilers remain temporary unless explicitly promoted as a future seed.

## Seed formatting and promotion

Generated Bootstrap C is formatted with the repository `.clang-format` policy. `scripts/format_seed_c.sh [path]` formats a generated C file in place, while `scripts/format_seed_c.sh --check [path]` verifies that it is already formatted without modifying it. The fixed-point runner performs the raw byte-level `n3.c == n4.c` check first, then formats both stable artifacts and checks that the formatted pair remains identical. This keeps formatting separate from self-hosting correctness.

After reviewing a stable fixed-point candidate, `scripts/promote_seed.sh [candidate.c]` copies it to `src/bootstrap/basaltc.seed.c`, runs the formatter, refreshes `src/bootstrap/fixed_point_production.sha256`, and refuses the default promotion if `.tmp/fixed-point/n3.c` and `n4.c` are not identical. The formatter is a presentation tool only; it does not participate in Basalt compilation or alter the frozen Host policy. A working environment that runs the formatting or promotion workflow must provide `clang-format` 18 or newer.

## CLI and process safety

The compatibility CLI is `basaltc [--line|--no-line] <input.basalt> [output.c]`. Source mapping is enabled by default for ordinary C generation; `--no-line` suppresses generated `#line` records without changing emitted program semantics. The explicit build form is `basaltc --compile <input.basalt> [-o <binary>] [--cc <compiler>] [-- <compiler-arguments...>]`. Auto-compile also keeps source mapping enabled by default; `--no-line` is the explicit opt-out and `--line` explicitly keeps it enabled. Only the repository's Bootstrap and fixed-point scripts pass `--no-line` while translating `src/bootstrap/basaltc.basalt`, so the frozen seed/current compiler artifacts do not carry mapping directives. This build policy is separate from the CLI behavior of the compiler when compiling user programs. The compiler arguments after `--` remain individual argv elements and are passed in order before the generated C path. Auto-compile MUST NOT concatenate user-provided options into a shell command.

The `sys::run` standard-library API applies the same rule to child processes. It receives an executable and `array::Array<string>` arguments, so spaces, quotes, and shell metacharacters remain data. It returns normalized status, success, bounded stdout/stderr, a truncation flag, and a spawn-error code. POSIX implementations use fork/exec, pipes, polling, and wait. Windows uses `CreateProcessW`, converts UTF-8 arguments to UTF-16, quotes each argv element directly, and drains separate stdout/stderr pipes concurrently; it never invokes `cmd.exe`. Both targets continue draining after their per-stream cap and wait for the child, so bounded capture does not create a full-pipe deadlock. There is no implicit shell mode.

## Validation policy

The Bootstrap-only suites cover strict GCC compilation, Clang compilation of the complete generated-C regression corpus, valid and invalid regression fixtures, controlled FFI ABI/header/ownership cases, the dedicated FFI portability gate, long-term conformance generation, ownership and move/borrow checks, pointer and function-pointer stress, adversarial sanitizer workloads, code-buffer growth, and fixed-point stability. Changes to CLI/source mapping/process execution additionally require tests for legacy positional output, both line modes, ordered compiler arguments, nonzero compiler status, whitespace/quote-preserving argv, empty arguments, nonzero child exit, missing executable, bounded output, and no shell injection through structured arguments. The memory sanitizer harness compares Bootstrap-generated C with a hand-written C baseline; it does not invoke the frozen Host compiler. FFI portability is reported only for compilers actually present: GCC and Clang strict C11 are required where configured, while MinGW is an object-only check and is skipped transparently when the toolchain is unavailable.

The compatibility script `scripts/check_parser_conflicts.sh` is retained only as a legacy entry point. It intentionally performs no OCaml, Dune, Menhir, or Host-parser operation.

## Super-test campaign

The `tests/super/` corpus is the pressure layer for changes that cross several compiler subsystems at once. It is intentionally separate from the smaller regression fixtures and is registered in `scripts/run_regression.sh`, so every ordinary regression run exercises it.

| Area | Coverage | Representative fixtures or checks |
|---|---|---|
| Integer boundaries | Maximum representable `u8`, `u16`, `u32`, and `u64` literals, plus a high-bit value | `integer_pointer_boundary_valid.basalt`, existing literal-overflow rejection fixtures |
| Pointer arithmetic | Same-array addition and subtraction, valid dereference, one-past pointer formation without dereference | `integer_pointer_boundary_valid.basalt`, pointer-arithmetic regression and stress fixtures |
| Generic specialization | Nested `Option`, `Result`, and dynamic `Array` instantiations; repeated calls with concrete element types | `stdlib_matrix_valid.basalt`, `closure_generic_nested_valid.basalt` |
| Closures and callbacks | Borrowed captures, function pointers, generic callback return substitution, and lifecycle checks | `closure_generic_nested_valid.basalt`, closure ownership fixtures |
| Standard library | Growth, indexing, higher-order operations, string helpers, map operations, and builder lifecycle | `stdlib_matrix_valid.basalt` and the existing standard-library matrix |
| Negative compilation | Generic element mismatch and incompatible generic callback signatures | `generic_element_mismatch_invalid.basalt`, `generic_callback_mismatch_invalid.basalt` |
| Toolchain safety | Strict C11 warnings, ASan, UBSan, fixed-point reproduction, and no repository ELF artifacts | `run_regression.sh`, `run_ownership_stress.sh`, and `fixed_point.sh` |

The valid super-tests are compiled from the current Bootstrap compiler and then built with `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` under GCC; the complete generated-C regression corpus is also exercised under Clang with the same profile. The sanitizer pass additionally uses AddressSanitizer and UndefinedBehaviorSanitizer with leak detection and fail-fast settings. Invalid fixtures must be rejected before a generated C file is accepted as a test artifact.

The test design follows the UBSan catalogue, which specifically calls out signed overflow, invalid shifts, null or misaligned dereferences, out-of-bounds indexing, pointer arithmetic overflow, and invalid indirect calls as runtime hazards [1]. It also follows CERT C INT32-C, which identifies arithmetic, compound assignment, shifts, and unary negation as signed-overflow risks and emphasizes values used in indexing, pointer arithmetic, and allocation sizes [2]. CERT C ARR30-C distinguishes a legal one-past pointer from an invalid dereference or out-of-range subscript; the boundary fixture forms the former and deliberately avoids the latter [3].

This campaign does not claim that a sanitizer run proves memory safety for every possible program. It establishes that the checked fixtures produce strict C11 and remain clean under the selected dynamic instrumentation, while compile-time negative cases exercise Basalt’s own type and ownership diagnostics.

## References

[1]: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html "Clang UndefinedBehaviorSanitizer documentation"

[2]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/integers-int/int32-c/ "SEI CERT C INT32-C"

[3]: https://cmu-sei.github.io/secure-coding-standards/sei-cert-c-coding-standard/rules/arrays-arr/arr30-c "SEI CERT C ARR30-C"
