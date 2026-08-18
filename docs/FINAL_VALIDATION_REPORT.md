# Final Validation Report

## Scope

This release completes the repository normalization and Bootstrap parity work for the Basalt compiler. The canonical self-hosting source is `src/bootstrap/basaltc.bsl`; no obsolete source extensions remain, and repository text contains no legacy branding or author/AI attribution.

## Bootstrap generic parity fix

The remaining failure was not caused by namespace lookup. Bootstrap successfully resolved declarations such as `map::new`, `map::put`, `result::ok`, and `slice::push`. The failure occurred during C generation: `gen_stmt` asks `gen_expr_kind` for print formatting, and the old `tc_expr_kind_for_emit` re-entered the type checker without the local-variable scope that had existed during the earlier function type-check pass. A variable argument in a later namespace-qualified generic call was therefore reported as an unknown name.

The canonical Basalt source now uses a side-effect-free emitter type inference path. Annotated variable expression types are read from the AST, generic return types are reconstructed through the emitter's existing binding machinery, and the code generator no longer mutates the type-checker's success state while determining output formatting.

Generic specialization collection also performs one warm-up generation pass followed by two generation passes whose token counts are compared. This accounts for dependent specializations discovered while traversing nested namespace-qualified generic calls. The second and third passes must agree before output is accepted.

## Validation results

| Check | Result |
| --- | --- |
| Host compiler build | Passed |
| Bootstrap C regeneration from `basaltc.bsl` | Passed |
| Strict GCC (`-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`) | Passed |
| `generic_map_probe.bsl` Host/Bootstrap parity | Passed |
| `result_option_test.bsl` Host/Bootstrap parity | Passed |
| `generic_slice_result_probe.bsl` Host/Bootstrap parity | Passed |
| Concrete `map`, `slice`, and container regression tests | Passed |
| Ownership valid and five negative fixtures | Passed; both compilers agree |
| ASan/UBSan and leak detection | Passed |
| Regression suite | Passed |
| Stress suite | Passed |
| Adversarial suite | Passed |
| Conformance suite | Passed |
| Fixed-point suite (`n2.c == n3.c`) | Passed |

The authoritative combined command is:

```sh
./scripts/run_ownership_stress.sh
```

It completed successfully after rebuilding both compiler paths and reported `PASS` for ownership, regression, stress, adversarial, conformance, and fixed-point validation.

## Repository normalization audit

| Audit item | Result |
| --- | --- |
| `.sl` files | 0 |
| Legacy branding tokens | 0 |
| Files containing author/AI attribution | 0 |
| Canonical Bootstrap source | `src/bootstrap/basaltc.bsl` |
| Generated Bootstrap C | `src/bootstrap/basaltc.bsl.c` |
| Executable files in release tree | 0 |
| Namespace stdlib modules | `src/stdlib/map.bsl`, `src/stdlib/slice.bsl`, `src/stdlib/result.bsl` |

The generated C remains a compile-time artifact of the compiler pipeline; the ownership checker and generic parity changes do not inject runtime bookkeeping into accepted programs.

## Reproduction

From the repository root:

```sh
(cd src/compiler && dune build bin/basaltc.exe)
src/compiler/_build/default/bin/basaltc.exe src/bootstrap/basaltc.bsl
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
  src/bootstrap/basaltc.bsl.c -o /tmp/basaltc
./scripts/run_ownership_stress.sh
```

The clean release archive excludes transient `.tmp` artifacts, executable files, and the Host compiler build directory while retaining the complete source tree, standard library, documentation, generated Bootstrap C, and full test corpus.
