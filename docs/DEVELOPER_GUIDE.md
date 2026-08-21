# Basalt Developer Guide

This guide explains how the Basalt repository is structured, how the two compilers cooperate, how the verification machinery works, and how to make changes safely — whether you are adding a standard-library module, a built-in function, or a language feature.

---

## 1. The two-compiler model

Basalt historically contained two implementations of the language pipeline, but the production workflow is now deliberately **Bootstrap-only**:

| Implementation | Language | Location | Status |
| --- | --- | --- | --- |
| Host | OCaml | `src/compiler/` | Frozen reference; never modified, built, or used for development validation |
| Bootstrap | Basalt | `src/bootstrap/basaltc.basalt` | Active self-hosting implementation |

A change is complete only when the Bootstrap compiler accepts valid programs, rejects invalid programs, emits strict-C11 C, reproduces its fixed point, and passes the full pressure suite. The frozen Host source is retained for historical reference and is outside the active change path.

The frozen seed C compiler is checked into `src/bootstrap/basaltc.seed.c` together with its SHA-256 (`src/bootstrap/fixed_point_production.sha256`). The seed must be regenerated from `basaltc.basalt` only after the two-stage fixed-point checks pass; generated binaries and intermediate C files remain under `.tmp/`.

The active implementation keeps lexer, parser, type checker, generator, and diagnostics in `basaltc.basalt`. Every language change is developed there, compiled from the frozen seed, validated through the current compiler, and then synchronized back into the seed artifact.

### The pipeline

| Stage | Responsibility |
| --- | --- |
| Lexer | Bytes → tokens (active Bootstrap `lexer_next`; frozen Host path retained only for reference) |
| Parser | Tokens → AST (active Bootstrap `ast_*`; frozen Host grammar retained only for reference) |
| AST | Deterministic Bootstrap node arenas |
| Type checker | Names, scopes, generics, fields, ownership, operator constraints |
| Specializer | Monomorphizes generic functions and types actually used |
| C generator | Emits an intermediate C-token stream, serializes to C11 |
| Validation | Strict GCC flags, sanitizers, fixed-point and pressure checks |

### Controlled C FFI

Controlled FFI is intentionally narrower than raw C injection. `extern func name(...): type;` remains backward-compatible, while `extern "header.h" func name(...): type;` records one validated header path for the generated C file. Header literals are non-empty and limited to ASCII letters, digits, `.`, `/`, `_`, and `-`; the emitter deduplicates identical controlled headers and writes quoted includes after the runtime prologue and before `includec` bytes.

The Bootstrap type checker rejects compiler-only representations at an extern boundary. Scalar types, pointers, fixed arrays, and named structs are accepted; dynamic arrays, tuples, variants, generic types, and other unsupported forms are rejected with stable diagnostic codes 55 (parameter), 56 (return), and 57 (header path). The compiler validates the Basalt declaration and generated C spelling, but the final C linker and platform ABI remain responsible for symbol availability and calling-convention compatibility.

The required implementation order is parser metadata, ABI validation, header registration, C emission, structured diagnostics, specification fixtures, regression registration, and fixed-point synchronization. Raw `includec` remains available for explicit C helper implementations and is not type-checked by Basalt.

### Ownership, borrowing, and lifetime analysis

Ownership is implemented entirely in the Bootstrap type checker. Parameter nodes store an ownership mode in `node_aux`: `0` means compatibility mode, `1` means `move`, `2` means shared `borrow`, and `3` means exclusive `borrow_mut`. The parser also creates `N_MOVE` expression nodes; the emitter deliberately lowers them to their child expression, because `move` is a compile-time transfer and must not duplicate evaluation or introduce runtime syntax.

The checker keeps ownership metadata beside the scope stack: moved state, owned state, parameter origin, shared-borrow count, mutable-borrow count, borrow source, and active parameter mode. Generic owner types are recognized recursively so `array::Array<T>` and related container specializations participate in the same state machine as dynamic arrays. Call checking applies the parameter mode uniformly to ordinary, generic, built-in, and indirect calls. A move-mode parameter accepts only an explicit `N_MOVE`; borrow modes resolve a tracked source and enforce shared/exclusive conflicts before the call proceeds.

Scope exit unwinds borrow counts associated with bindings declared in that scope. Return checking propagates the source of pointer expressions and rejects pointers derived from block-local owners while allowing globals and formal parameters under lifetime elision. This is a conservative escape analysis: it does not inspect arbitrary C inside `includec`, and raw FFI pointers remain an explicit unsafe boundary. Ownership errors use stable diagnostics 33, 35, 37, 38, 40, 58, 59, 60, and 61; new fixtures must assert the semantic code and source excerpt.

### Captured closures and generated ABI

Closure syntax is parsed into `N_CLOSURE` with four payloads: the linked capture list in `node_a`, the body statement in `node_b`, the parameter list in `node_c`, and the declared return type in `node_value`. Capture mode remains in each capture node's `node_aux`: `1` for move, `2` for borrow, and `3` for borrow_mut. The type checker creates an inner scope, installs capture aliases and parameters, checks the body against the declared return type, and produces a structural `TY_CLOSURE` signature.

The generator registers each closure literal deterministically during collection. A closure signature gets one shared fat-value struct and one call wrapper; each literal gets a serial-specific environment, factory, and lifted invoke function. The generated shape is equivalent to:

```c
struct __basalt_closure_value_SIG {
  void *env;
  RET (*fn)(void *, PARAMS);
};
```

Factories allocate and initialize environments through the existing tracked allocator. Lifted invokes receive a typed environment pointer before user parameters. Borrowed captures are emitted as dereferenced environment fields, while moved captures are emitted as values stored in the environment. Capture-free environments contain a dummy field so the generated C remains valid on all conforming C11 implementations. Closure calls dispatch through the fat value's `fn` and `env` members; ordinary function pointers continue to use the existing indirect-call path.

The closure registry is reset before each generation and populated before closure ABI declarations are emitted. This ordering is important: an environment may store another closure value by value, so shared closure-value structs must be complete before environment definitions. Any change to closure collection or emission must test repeated generation, nested captures, empty environments, strict GCC, and the no-artifact-on-rejection invariant.

A closure borrow that escapes its source binding reports diagnostic **60**. A source binding used after a move capture reports diagnostic **61**. Both errors are checked before C emission.

Any ownership or closure change must be developed in `src/bootstrap/basaltc.basalt`, rebuilt from the frozen seed with strict C11 flags, exercised by `scripts/run_ownership_stress.sh`, and only then synchronized into `basaltc.seed.c` and `fixed_point_production.sha256`. Do not modify, build, or use `src/compiler/` for this workflow.

---

## 2. Repository layout

```
src/compiler/            Frozen OCaml reference (do not modify, build, or use)
src/bootstrap/           basaltc.basalt (source), basaltc.seed.c (checked-in seed),
                         fixed_point_production.sha256
src/stdlib/              array.basalt, slice.basalt, map.basalt, option.basalt, result.basalt, string.basalt
tests/regression/        focused compiler + language tests (valid and expect-reject)
tests/stress/            larger corpus (164 cases), move/borrow fixtures, modulo stress
tests/conformance/       Host vs Bootstrap conformance material
tests/adversarial/       sanitizer-oriented tests (ASan/UBSan)
tests/benchmark/         cross-language benchmark sources
tests/regression/*.c     tracked includec helper runtimes (intentional)
docs/                    spec, design, guides, reports
scripts/                 all build/verify commands

For Windows UCRT64 bootstrap compilation, see [`WINDOWS_UCRT64_BOOTSTRAP.md`](WINDOWS_UCRT64_BOOTSTRAP.md) for the pthread compatibility shim, `-pthread`/`-lpthread` linking guidance, and the `aligned_alloc` declaration.
```

---

## 3. The verification machinery

### 3.1 Build the current Bootstrap compiler

```sh
source scripts/bootstrap_stage.sh
current_bin=$(bootstrap_stage "$PWD" .tmp/developer-stage \\
  -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
```

This builds the frozen `src/bootstrap/basaltc.seed.c` and uses it to translate `src/bootstrap/basaltc.basalt`. It never enters `src/compiler/`.

### 3.2 Verify the Bootstrap fixed point

```sh
./scripts/fixed_point.sh
```

The script:

1. Builds `n1.bin` directly from the frozen `src/bootstrap/basaltc.seed.c` with the strict GCC profile.
2. Runs `n1.bin basaltc.basalt n2.c`, then builds `n2.bin` from `n2.c`.
3. Runs `n2.bin basaltc.basalt n3.c`, then builds `n3.bin` from `n3.c`.
4. Runs `n3.bin basaltc.basalt n4.c` and requires `n3.c == n4.c` byte-for-byte.
5. Verifies the frozen seed checksum against `fixed_point_production.sha256`; after a validated change, the stable n3 artifact and checksum are synchronized back into the seed.

A passing fixed point proves the self-hosting compiler is stable: the compiler built by itself produces the same compiler artifact.

**After changing `basaltc.basalt`, you must** run the Bootstrap-only fixed-point chain, synchronize the stable n3 artifact, and rerun the chain:

```sh
bash scripts/fixed_point.sh
cp .tmp/fixed-point/n3.c src/bootstrap/basaltc.seed.c
sha256sum src/bootstrap/basaltc.seed.c | awk '{print $1}' > \\
  src/bootstrap/fixed_point_production.sha256
bash scripts/fixed_point.sh
```

Only after the synchronized fixed point passes may the full ownership-stress suite be run and the change committed. All generated binaries and intermediate C files remain under `.tmp/`.

### 3.3 The full suite

```sh
./scripts/run_ownership_stress.sh
```

This is the master runner. It builds the current compiler from the frozen C seed and executes the Bootstrap-only validation stages in order:

1. **Ownership stress**: move/borrow valid and negative fixtures under strict GCC, ASan, UBSan, and leak checks.
2. **Regression** (`scripts/run_regression.sh`): valid fixtures must compile, pass strict GCC, and run; invalid fixtures must be rejected before C emission.
3. **Stress** (`scripts/run_stress.sh`): the larger corpus plus modulo stress.
4. **Adversarial** (`scripts/run_adversarial.sh`): sanitizer-driven workloads.
5. **Conformance** (`scripts/run_conformance.sh`): Bootstrap-generated conformance material against checked-in expectations; it does not build the frozen Host.
6. **Fixed point**: `fixed_point.sh`.
7. **ELF guard**: no executable binaries may be left under `tests/` (all artifacts live in `.tmp/`).

If you add a fixture, register it in `scripts/run_regression.sh`:

```sh
compile_run "$ROOT/tests/regression/my_feature.basalt" my_feature          # valid program
expect_reject "$ROOT/tests/regression/my_feature_invalid.basalt" my_feature_invalid  # Bootstrap must reject before C emission
```

`compile_run` compiles the fixture with the current Bootstrap compiler from the fixture's directory (so relative `include` paths resolve), then requires strict-GCC acceptance and a successful run. `expect_reject` requires rejection before a generated C artifact is accepted. The frozen Host implementation is not invoked.

---

## 4. Adding a built-in function

Built-ins live in two tables that must agree:

- Bootstrap: `bi_init()` in `src/bootstrap/basaltc.basalt` — a data-driven registry built from `bi_register(name, tc_tag, flags)`.
- The OCaml tables under `src/compiler/` are historical reference material only and must not be edited or used as part of a feature change.

The registry replaced a set of hardcoded name-hash checks. Tags: `BI_TC_NONE/VOID/INT/STRING/PTR_INT/PTR_VOID/MEM_ALLOC/MEM_RESIZE/MEM_FREE`; flags: `BI_FLAG_RESERVED/OWNED/CONSUME/DYNFIELD/MAIN`. Lookup is by `sym_len` + `sym_hash`, matching what the old code did, but adding a built-in is now a table entry instead of touching five functions.

**Worked example — exposing `basalt_inc_join`** (a runtime helper that was reserved but not callable):

1. Bootstrap: one line in `bi_init()`:

   ```basalt
   bi_register("basalt_inc_join", BI_TC_STRING, BI_FLAG_RESERVED);
   ```

2. Add a fixture (`tests/regression/builtin_join_test.basalt`) and register it.

That is the whole change. Before the registry, the same addition touched the reserved list, the type-check dispatch, the emitter dispatch, and the ownership tables — five independent hardcoded hash lists that could drift apart.

**Design notes for the registry** (learned the hard way):

- `bi_init()` is **lazy**: it runs on the first `bi_lookup` during type checking. Eager initialization in `main()` fails because type-checking `bi_register`'s own body needs the registry (`grow_ints`) — a circular dependency.
- `bi_register` interns names into the **symbol-text region** (`source_len + sym_text_len`), exactly like `sym_qualified`. Writing at plain `source_len` **overwrites the qualified-name region** used by namespace symbols (e.g. `map::free`) and breaks resolution for programs longer than the registry's footprint. This was a real bug: `stdlib_containers_test.basalt` failed with `unknown function` at a line inside the map section while short programs passed.
- `bi_lookup` compares `sym_len` and `sym_hash` only — never the source text — so the comparison is immune to later buffer reuse.
- A built-in that is reserved but not callable (`printf`, `malloc`, `strlen`, ...) gets `BI_TC_NONE`. `write_int` was originally registered with `BI_TC_INT` by mistake; it is reserved-only and must remain `BI_TC_NONE` in the active Bootstrap registry.

**Adding a *new* runtime helper** requires updating the Bootstrap runtime-prologue emission and its Bootstrap-only fixtures. Keep the generated prologue centralized and verify that each required header or helper is emitted exactly once.

---

## 5. Adding a standard-library module

Standard-library modules are plain Basalt in `src/stdlib/`, using the proven patterns: `namespace`, generic `struct`, zero-initialization (`= 0`), and accessor functions.

**Worked example — `option.basalt`** (null-safety module):

```basalt
namespace option {
  struct Option<T> {
    present: int;
    value: T;
  }

  func some<T>(value: T): Option<T> {
    let o: Option<T> = 0;
    o.present = 1;
    o.value = value;
    return o;
  }

  func none<T>(zero: T): Option<T> {
    let o: Option<T> = 0;
    o.present = 0;
    o.value = zero;
    return o;
  }

  func is_some<T>(o: Option<T>): int { return o.present; }
  func is_none<T>(o: Option<T>): int { return o.present == 0; }

  func unwrap_or<T>(o: Option<T>, fallback: T): T {
    if o.present == 1 then { return o.value; }
    return fallback;
  }
}
```

Rules of thumb:

- Keep the API **total** — no functions that can panic. `unwrap` was deliberately omitted.
- `none` requires a zero value of `T` because every struct is fully materialized.
- Add a fixture that exercises **every** function, including nested generic calls: `option::unwrap_or(option::none("zero"), "fallback")`.

### Stabilization contract for Option, Result, and containers

The canonical `option` module and the `result` module use fully materialized generic structs. Every constructor initializes both the active and inactive payloads, including the explicit `zero` witness required by Bootstrap for a generic value. This makes predicates, fallback access, mapping, filtering, and composition total without panic paths or reads from uninitialized storage.

`option::Option<T>` exposes `some`, `none`, `is_some`, `is_none`, `unwrap_or`, `value_or`, `map`, `map_or`, `filter`, `contains`, `or_else`, and `and_option`. `result::Result<T,E>` exposes `ok`, `err`, `is_ok`, `is_err`, `unwrap_or`, `value_or`, `error_or`, `map`, `map_or`, `map_error`, `map_error_or`, `contains`, `and_result`, and `or_result`. Higher-order functions use the existing `fn(...)` function-pointer type; they must not be reimplemented as closure-only APIs.

The old `result::Option<T>` surface remains intentionally available for source compatibility. New modules and examples should include `option.basalt` and use `option::Option<T>`. Compatibility aliases are preferred over breaking renames while the standard library is still being stabilized.

Container modules follow the same low-surprise conventions. `array::len`, `slice::len`, `map::len`, and `string_builder::len` are stable aliases for the established length operations; `slice::map` and `slice::filter` return initialized values by value; `map::is_empty` and the corresponding container helpers report state without exposing internal capacity. Growth, hashing, ownership, and cleanup remain inside their existing implementations rather than being duplicated in compatibility wrappers.

Every standard-library change must be exercised in three ways: a normal fixture covering each public function, an edge fixture covering empty/absent/out-of-range behavior and zero-length cleanup, and the full Bootstrap regression and sanitizer suites. Generic tests must include nested calls and at least two distinct element/error types so specialization and inactive-payload initialization are both tested.

**Nested generic calls exposed a real compiler bug.** The fixture above was the first program to call a generic function inside the argument of another generic function. The Host collected instantiations recursively (`compiler.ml` `scan_expr`), but the Bootstrap's `gen_collect_expr` only recursed into arguments for *non-generic* calls — generic calls computed their own spec and then returned, so `option__none__char_ptr` was never generated and the emitted C failed to compile. Fix: always recurse into arguments after computing the generic spec:

```basalt
if k == N_CALL then {
  let f: int = tc_find_function_ctx(node_value[id], node_scope[id]);
  if f != 0 && node_kind[f] == N_GENERIC_FUNC then {
    ... compute `actual` and gen_add_fun_spec(f, actual) ...
  }
  let aar: int = node_a[id];
  while aar != 0 { gen_collect_expr(aar); aar = node_next[aar]; }
  return;
}
```

The lesson: **every new fixture is also a compiler test** — the first run of a new fixture found a bug no existing test had exercised.

---

## 6. Adding a compiler feature

The typical workflow for a language-level change, demonstrated by the compile-time bounds check for fixed arrays:

1. **Host typechecker** (`typechecker.ml`): add the rule. A `const_int` helper recognizes both `5` and the `0 - 1` form of `-1`; indexing `TArray (t, n)` with a constant outside `[0, n)` yields `array index K out of bounds for length N`. Also relax the initializer rules so `let a: int[3] = 0;` is legal (local and global) — previously no fixed array could be declared at all.
2. **Host emitter** (`compiler.ml`): emit `= {0}` for fixed-array initializers (previously the initializer was dropped, diverging from the Bootstrap, which emitted `= 0`).
3. **Bootstrap** (`basaltc.basalt`): mirror the check in `tc_expr` using `node_value[tc_result_type]` as the array size, with `tc_fail(45)` and a message. Native `<=` and `>=` comparisons are supported by the Bootstrap lexer, parser, type checker, and emitter; unary `!` is represented as `N_UNARY` and produces a boolean result. Mirror the initializer emission in `gen_initializer` (`{0}`).
4. **Fixtures**: `fixed_array_valid` (boundary indexes), `fixed_array_oob_literal`, `fixed_array_oob_negative`, `fixed_array_oob_struct_field`; register the valid one with `compile_run` and the others with `expect_reject`.
5. **Verify**: full suite, then regenerate the bootstrap artifact + checksum, then the suite again.

### Example of a Host-only bug found while writing documentation

The enum fixture `tests/regression/enum_typechecker_valid.basalt` existed but was **not registered** in any suite — and it failed on the Host compiler with `internal: unknown variable or function Green` while the Bootstrap accepted it. Two Host defects surfaced:

1. `emit_expr_type` (used during codegen) looked up `Var` in `vars` and `funcs` but not in **enum values**, although the type checker already resolved them. Fix: add `enum_values : typ SMap.t` to the emit environment, populate it from `program.enums`, and fall back to it in `emit_expr_type`.
2. The Host emitted `typedef enum Color Color;` as a *forward declaration* before the definition. ISO C11 forbids forward references to `enum` types (unlike `struct`/`union`), which failed under `-Wpedantic -Werror`. Fix: emit `typedef enum Color Color;` immediately **after** each enum definition instead.

The fixtures were then registered in `run_regression.sh` so the gap cannot silently reopen.

**Checklist for any compiler change:**

- [ ] Both compilers updated (or a deliberate, documented divergence)
- [ ] Valid fixture that exercises the feature positively
- [ ] Negative fixture that must be rejected by **both** compilers
- [ ] Fixtures registered in `scripts/run_regression.sh`
- [ ] `./scripts/run_ownership_stress.sh` fully green
- [ ] Bootstrap artifact + checksum regenerated if `basaltc.basalt` changed
- [ ] Documentation updated (`docs/`, `README.md`)

---

## 7. Conventions

- **Commit style**: imperative subject line, sentence case, no trailing period; body explains what and why. Examples from the repository history:
  - `Compile-time bounds checks for fixed arrays; allow int[n] = 0 initializers`
  - `Data-driven builtin registry in bootstrap; replace hardcoded name hashes with a runtime table; expose basalt_inc_join`
  - `Add option.basalt null-safety stdlib; fix bootstrap generic call collection for nested calls in arguments`
- **Diagnostics**: the Host prints `Type Error: <message>`; the Bootstrap prints `type error: <code>` (with a message for most codes). Codes are shared and must stay in sync.
- **Files**: sources are `.basalt`; generated C is `*.basalt.c` (gitignored except the bootstrap artifact); fixtures with a runtime component carry a tracked `.c` partner used through `includec`.
- **Artifacts**: everything generated during verification lives under `.tmp/`; `tests/` must never contain executables.

---

## 8. Troubleshooting

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| `Fixed-point checksum mismatch` | `basaltc.basalt` changed without regenerating artifacts | Rebuild bootstrap, update `basaltc.basalt.c` + checksum |
| `FAIL regression` for a new fixture | Fixture not registered, or relative `include` path wrong | Register in `run_regression.sh`; includes resolve relative to the fixture's directory |
| `unknown function` / `invalid expression` on long programs | Symbol-text region overwritten (registry-era bug) or missing nested generic collection | Check `bi_register` region usage; check `gen_collect_expr` recursion |
| `cc1: ... error:` after `-Werror` | Emitted C violates strict C11 (e.g. enum forward typedef) | Fix the emitter; run the emitted C through the strict profile |
| `FAIL: stray ELF binaries under tests/` | A build wrote an executable into `tests/` | Move outputs to `.tmp/`; remove the binary |
| Host and Bootstrap disagree on a fixture | Divergence introduced by the change | Mirror the fix in the other compiler; both must reject/accept identically |

---

## 9. Reading order

1. `README.md` — overview and quick start
2. `docs/USER_GUIDE.md` — the language and stdlib from a user's perspective
3. `docs/LANGUAGE_SPEC.md` — formal syntax and semantics
4. `docs/DESIGN.md` — architecture notes
5. `docs/NAMING.md` — repository naming policy
6. `docs/MEMORY_SAFETY_AUDIT.md` — ownership and runtime memory model
7. This guide — for contributors