# Pyrel Memory-Safety Audit and Ownership-Checking Status

## Executive assessment

Pyrel now provides **static move/borrow checking for dynamic arrays (`TDynArray`) in both the Host and Bootstrap compilers**, in addition to the previously implemented runtime allocation registry and checked dynamic-array operations. The checker follows a deliberately narrow and auditable rule set: dynamic arrays are owning values, ownership moves by default across recognized transfers, address-taking creates a scoped borrow, mutation and release are forbidden while a borrow is active, and a borrow cannot escape through a pointer return.[^1][^2]

This milestone does not make every pointer operation memory-safe. Raw pointers, pointer arithmetic, arbitrary dereference, `extern`, `includec`, and unconstrained C interoperability remain explicit low-level or unsafe boundaries. The ownership guarantee is therefore precise rather than overstated: **the implemented static contract covers dynamic-array ownership and address-derived borrows, while legacy pointer handles remain subject to runtime registry validation and explicit release tracking**.

The implementation obeys the Pyrel dogfooding rule. The Bootstrap checker was modified in Pyrel source, regenerated to C, rebuilt with strict GCC, and used to regenerate the next Bootstrap generation. The Host OCaml checker was kept in parity with the Pyrel implementation and was changed only for compiler-side ownership behavior and previously confirmed platform mismatches.[^3]

## Static ownership model

> **Owner.** A `TDynArray` value owns its allocation and may be moved, but it may not be implicitly copied.
>
> **Borrow.** `&x` records a scoped borrow of `x`; the borrow ends when the pointer variable leaves scope or is reassigned.
>
> **Move.** A successful ownership transfer invalidates the source. A later use, second move, or release is rejected at compile time.
>
> **Borrow conflict.** Mutation, release, or ownership transfer of a borrowed source is rejected until all derived borrows have ended.

The checker records per-variable ownership and move state, borrow counts, and borrow provenance. Bootstrap uses parallel state arrays and explicit scope unwinding; Host uses the corresponding `owned`, `moved`, `borrow_counts`, and `borrow_sources` environment fields. Function parameters of dynamic-array type are treated as owned by the callee. A direct `let b: array<T> = a` is rejected as an implicit owner copy; constructors and recognized ownership-producing calls remain valid initializers.[^1][^2]

The built-in release path for `array_free` consumes the dynamic-array owner and rejects a later release. Legacy allocator handles such as `alloc_ints`, `open_file`, and include-file handles retain the existing runtime release discipline; their static tracking is kept consistent with the established Host behavior so Bootstrap self-hosting does not misclassify a locally acquired handle as borrowed.[^1][^2]

## Implemented mechanisms

| Area | Implemented behavior | Status |
| --- | --- | --- |
| Dynamic-array owner state | `TDynArray` values are marked owned at declaration or recognized constructor result; moved sources become invalid | Implemented in Host and Bootstrap |
| Function argument transfer | Dynamic-array arguments passed to owned parameters are consumed exactly once | Implemented in Host and Bootstrap |
| Owner copy prevention | `let b: array<T> = a` is rejected with the explicit-move diagnostic | Implemented in Host and Bootstrap |
| Use after move | Reads, second moves, and later releases of moved dynamic arrays are rejected | Implemented in Host and Bootstrap |
| Borrow provenance | `&owner`, pointer copies, dereference, and dynamic-array indexing preserve borrow provenance where applicable | Implemented in Host and Bootstrap |
| Borrow conflict | Mutation, release, or move of a source with an active borrow is rejected | Implemented in Host and Bootstrap |
| Borrow scope unwind | Borrow counts are released when pointer variables leave their block scope or are reassigned | Implemented in Host and Bootstrap |
| Borrow escape | Returning `&local` through a pointer return is rejected | Implemented in Host and Bootstrap |
| Release state | `array_free` consumes the owner and catches a second release at compile time | Implemented in Host and Bootstrap |
| Runtime allocation registry | Duplicate registration, unknown release, growth overflow, and invalid live-state transitions fail closed | Implemented in Host and Bootstrap |
| Checked dynamic-array access | Reads and writes validate lifetime, bounds, shape, and byte-offset arithmetic | Implemented in Host and Bootstrap |
| String and integer allocation hardening | Allocation size and growth arithmetic are checked before allocation or copy | Implemented in Host and Bootstrap |

## Diagnostics and parity

Both compilers reject every ownership-negative fixture in the new corpus. The acceptance contract is byte-independent of diagnostic wording: a fixture is considered parity-safe only when **both Host and Bootstrap reject it with a nonzero status**, while valid programs must be accepted, strict-built, and produce identical output.[^4]

| Fixture | Host result | Bootstrap result | Semantic case |
| --- | --- | --- | --- |
| `move_borrow_valid.pyrel` | Accepted; output `7 8 41 41` | Accepted; output `7 8 41 41` | Valid move, scoped borrow, and post-borrow mutation |
| `move_borrow_invalid_use_after_move.pyrel` | Rejected | Rejected | Read after passing owner to an owned parameter |
| `move_borrow_invalid_double_free.pyrel` | Rejected | Rejected | Release after the first `array_free` consumed the owner |
| `move_borrow_invalid_mutate_borrowed.pyrel` | Rejected | Rejected | Mutation while an address-derived borrow is active |
| `move_borrow_invalid_borrow_escape.pyrel` | Rejected | Rejected | Returning a pointer to a local owner |
| `move_borrow_invalid_owner_copy.pyrel` | Rejected | Rejected | Implicit dynamic-array owner copy |

The semantic results are identical, while the current human-readable diagnostics are not yet byte-for-byte identical. Host reports, for example, `cannot mutate borrowed value` and `borrow escapes function through return`; Bootstrap reports the equivalent messages `cannot mutate or move while borrowed` and `borrowed reference escapes its owner`. The error classes are aligned, including Bootstrap's explicit mapping for error 33 (`use after ownership move`) and error 40 (`owned value copy requires an explicit move`).

## Confirmed Host runtime bug and fix

Before the Host emitter hardening, direct dynamic-array indexing could reach raw memory even though Bootstrap rejected the same out-of-bounds access. The Host emitter now routes dynamic-array reads through the checked runtime helper, aligning the two compiler paths. The adversarial fixture `memory_oob_test.pyrel` is compiled through both paths and both generated programs terminate with the same deterministic failure status without AddressSanitizer or UBSan diagnostics.[^5]

## Validation evidence

| Check | Result | Evidence |
| --- | --- | --- |
| Host Dune build | Passed | `pyrel/scripts/run_ownership_stress.sh` |
| Ownership valid fixture | Host and Bootstrap accepted | `tests/stress/move_borrow_valid.pyrel` |
| Ownership negative corpus | Five fixtures rejected by both compilers | `tests/stress/move_borrow_invalid_*.pyrel` |
| Strict generated C | Passed with `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror` | Ownership runner and existing suites |
| Ownership sanitizer run | ASan and UBSan passed with leak detection enabled | `run_ownership_stress.sh` |
| Existing regression suite | Passed | `run_regression.sh` |
| Existing stress corpus | Passed, including the prior corpus | `run_stress.sh` |
| Adversarial sanitizer suite | Passed, including deterministic OOB rejection | `run_adversarial.sh` |
| Conformance suite | Passed | `run_conformance.sh` |
| Fixed point | Passed; successive generated C matched | `fixed_point.sh` |
| C before/after comparison | No textual or byte difference for the valid ownership fixture; both files are 9,687 bytes with SHA-256 `e21ef2547e65b93724b28ab20e35bba81541820da25982783cb7eb1ea258a692` | `.tmp/c_before_after/summary.txt` and `sha256.txt` |

The unchanged C output is expected. Ownership checking is a compile-time acceptance analysis; it does not inject runtime bookkeeping into an accepted program. The meaningful change is that invalid ownership programs no longer reach code generation, while valid programs preserve the established emitter output.

## Reproducible verification commands

From the repository root, the complete ownership and compatibility validation is:

```sh
cd pyrel
./scripts/run_ownership_stress.sh
```

The runner builds the Host compiler, strict-builds the canonical Bootstrap C artifact, checks the valid and invalid ownership fixtures, compiles valid outputs under strict GCC and ASan/UBSan, compares runtime output, and then invokes the regression, stress, adversarial, conformance, and fixed-point suites. Generated logs and binaries are written below `pyrel/.tmp/` and are not part of the clean package.

## Remaining limitations

The checker is intentionally not presented as a Rust-equivalent ownership system. Ownership of struct fields, array elements, arbitrary pointer-derived aliases, function-pointer captures, and values crossing unconstrained FFI boundaries is not represented with complete provenance. Pointer arithmetic remains low-level, and `extern`/`includec` declarations cannot infer C-side nullability, ABI layout, ownership transfer, or lifetime guarantees.

The next language-safety milestones should introduce explicit ownership annotations for FFI and pointer-returning functions, broaden provenance through field and element projections, and add a first-class unsafe boundary for pointer arithmetic and raw C interoperation. Those extensions should preserve the current Bootstrap-first workflow and retain differential Host/Bootstrap testing as a release gate.

## References

[^1]: [Bootstrap ownership checker source](../src/bootstrap/pyrelc.pyrel)
[^2]: [Host ownership checker source](../src/compiler/lib/typechecker.ml)
[^3]: [Bootstrap fixed-point verification script](../scripts/fixed_point.sh)
[^4]: [Ownership stress runner](../scripts/run_ownership_stress.sh)
[^5]: [Adversarial sanitizer runner](../scripts/run_adversarial.sh)

