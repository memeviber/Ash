# Ownership Validation Report

## Scope

This report records the final validation of the Pyrel static move/borrow checker in the Host and Bootstrap compilers. The checked owner type is `array<T>`/`TDynArray`; pointer borrows created by address-taking are tracked for scope, mutation, release, move, and return-escape rules.

## Ownership corpus

| Case | Host | Bootstrap | Expected |
| --- | --- | --- | --- |
| `move_borrow_valid.pyrel` | Accepted | Accepted | Runtime output must match |
| `move_borrow_invalid_use_after_move.pyrel` | Rejected, status 1 | Rejected, status 1 | Compile-time rejection |
| `move_borrow_invalid_double_free.pyrel` | Rejected, status 1 | Rejected, status 1 | Compile-time rejection |
| `move_borrow_invalid_mutate_borrowed.pyrel` | Rejected, status 1 | Rejected, status 1 | Compile-time rejection |
| `move_borrow_invalid_borrow_escape.pyrel` | Rejected, status 1 | Rejected, status 1 | Compile-time rejection |
| `move_borrow_invalid_owner_copy.pyrel` | Rejected, status 1 | Rejected, status 1 | Compile-time rejection |

The valid fixture prints the identical output `7 8 41 41` through both compiler paths. Its generated C passes strict GCC with `-std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror`.

## Sanitizer and regression result

The valid ownership fixture was rebuilt with AddressSanitizer and UndefinedBehaviorSanitizer, using `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1`. Both Host and Bootstrap executions completed without sanitizer or leak diagnostics.

The final command was:

```sh
cd pyrel
./scripts/run_ownership_stress.sh
```

The command passed the ownership corpus, the existing regression suite, the 164-case stress corpus plus modulo coverage, the adversarial sanitizer suite including deterministic out-of-bounds rejection, the conformance suite, and the fixed-point suite.

## Generated C before/after

The valid ownership fixture was generated with the pre-checker Bootstrap compiler and the ownership-enabled Bootstrap compiler. Both outputs are 9,687 bytes and are byte-identical.

| Artifact | SHA-256 |
| --- | --- |
| C before checker | `e21ef2547e65b93724b28ab20e35bba81541820da25982783cb7eb1ea258a692` |
| C after checker | `e21ef2547e65b93724b28ab20e35bba81541820da25982783cb7eb1ea258a692` |

The unified diff is empty. This confirms that the ownership checker changes acceptance behavior without changing emitted C for an accepted program.

## Fixed point

After synchronizing `pyrelc.pyrel` with `pyrelc.pyrel`, the fixed-point script verified `n2.c == n3.c`. The production checksum recorded in `src/bootstrap/fixed_point_production.sha256` is:

```text
5f6c9186e4eb35d60062285d0becae0b13a671e097a9abdb2d32c0fc0866f004
```

## Artifacts

The detailed runner logs are generated below `pyrel/.tmp/ownership_stress/` and are intentionally excluded from the clean ZIP. The repository contains the reproducible runner, fixtures, Bootstrap source and generated artifact, Host checker, audit, release notes, and this report.

