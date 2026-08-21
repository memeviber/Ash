# Basalt Developer Guide

This guide explains how the Basalt repository is structured, how the two compilers cooperate, how the verification machinery works, and how to make changes safely — whether you are adding a standard-library module, a built-in function, or a language feature.

---

## 1. The two-compiler model

Basalt contains **two complete implementations** of the same language pipeline:

| Implementation | Language | Location | Purpose |
| --- | --- | --- | --- |
| Host | OCaml | `src/compiler/` | Reference implementation |
| Bootstrap | Basalt | `src/bootstrap/basaltc.basalt` | Self-hosting implementation |

The **parity rule**: a change is complete only when both compilers

1. accept the same valid programs,
2. reject the same invalid programs,
3. emit compilable, strict-C11 C,
4. produce the same runtime behavior.

The Bootstrap compiler's generated C is checked into the repository as `src/bootstrap/basaltc.basalt.c` together with its SHA-256 (`src/bootstrap/fixed_point_production.sha256`). The two artifacts must stay in lockstep with `basaltc.basalt`.

The two implementations are deliberately kept structurally close: same pipeline stages, same AST shapes, same diagnostics codes. When a language change lands, the Host is usually changed first (it is easier to edit), then the Bootstrap mirrors it, and finally the fixed-point and suite machinery proves the two agree.

### The pipeline

| Stage | Responsibility |
| --- | --- |
| Lexer | Bytes → tokens (`src/compiler/lib/lexer.mll`, bootstrap `lexer_next`) |
| Parser | Tokens → AST (`src/compiler/lib/parser.mly`, bootstrap `ast_*`) |
| AST | Deterministic node arenas (both compilers) |
| Type checker | Names, scopes, generics, fields, ownership, operator constraints |
| Specializer | Monomorphizes generic functions and types actually used |
| C generator | Emits an intermediate C-token stream, serializes to C11 |
| Validation | Strict GCC flags, sanitizers, differential comparison |

---

## 2. Repository layout

```
src/compiler/            OCaml Host compiler (lexer.mll, parser.mly, ast.ml,
                         typechecker.ml, compiler.ml, parser.conflicts)
src/bootstrap/           basaltc.basalt (source), basaltc.basalt.c (checked-in artifact),
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

### 3.1 Build the Host

```sh
./scripts/build.sh
```

Runs `dune build` in `src/compiler/` and then checks parser conflict counts (`check_parser_conflicts.sh`) so grammar changes cannot silently grow the conflict set.

### 3.2 Verify the bootstrap fixed point

```sh
./scripts/fixed_point.sh
```

The script:

1. Host-compiles `src/bootstrap/basaltc.basalt` → `basaltc.basalt.c` (overwriting the working copy, **not** the checked-in one).
2. Builds `n1.bin` from that C with the strict GCC profile.
3. Runs `n1.bin basaltc.basalt n2.c`, builds `n2.bin` from `n2.c`.
4. Runs `n2.bin basaltc.basalt n3.c`.
5. Requires `n2.c == n3.c` byte-for-byte, and `sha256sum n2.c == fixed_point_production.sha256`.

A passing fixed point proves the self-hosting compiler is stable: the compiler built by itself produces the same compiler artifact.

**After changing `basaltc.basalt`, you must** regenerate the checked-in artifacts. The exact sequence used by maintainers:

```sh
./scripts/build.sh
src/compiler/_build/default/bin/basaltc.exe src/bootstrap/basaltc.basalt
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \
  src/bootstrap/basaltc.basalt.c -o .tmp/n1.bin
.tmp/n1.bin src/bootstrap/basaltc.basalt .tmp/n2.c
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror .tmp/n2.c -o .tmp/n2.bin
.tmp/n2.bin src/bootstrap/basaltc.basalt .tmp/n3.c
cmp .tmp/n2.c .tmp/n3.c && sha256sum .tmp/n2.c
```

Then write the new hash into `src/bootstrap/fixed_point_production.sha256` and commit the new `basaltc.basalt.c` and checksum together with the `.basalt` change. If the checksum does not change while the artifact does, something is wrong.

### 3.3 The full suite

```sh
./scripts/run_ownership_stress.sh
```

This is the master runner. It builds both compilers and executes, in order:

1. **Ownership stress**: the move/borrow valid fixture (compiled by both compilers, strict GCC, ASan + UBSan + leak check, identical runtime output) and five negative fixtures that **both** compilers must reject.
2. **Regression** (`scripts/run_regression.sh`): `compile_run` fixtures (both compilers must compile, pass strict GCC, and run) and `expect_reject` fixtures (both must reject). Also checks the runtime prologue is emitted exactly once for include tests.
3. **Stress** (`scripts/run_stress.sh`): the 164-case corpus plus modulo stress.
4. **Adversarial** (`scripts/run_adversarial.sh`): sanitizer-driven tests; the OOB negative fixture must terminate with exit code `2` in both compiler paths.
5. **Conformance** (`scripts/run_conformance.sh`): generated Host/Bootstrap conformance material.
6. **Fixed point**: `fixed_point.sh`.
7. **ELF guard**: no executable binaries may be left under `tests/` (all artifacts live in `.tmp/`).

If you add a fixture, register it in `scripts/run_regression.sh`:

```sh
compile_run "$ROOT/tests/regression/my_feature.basalt" my_feature          # valid program
expect_reject "$ROOT/tests/regression/my_feature_invalid.basalt" my_feature_invalid  # both compilers must reject
```

`compile_run` compiles the fixture with the Host from the fixture's directory (so relative `include` paths resolve), then with the Bootstrap, then requires strict-GCC acceptance and a successful run of both binaries.

---

## 4. Adding a built-in function

Built-ins live in two tables that must agree:

- Host: `Ast.builtin_funcs` in `src/compiler/lib/ast.ml` (type signature) and `reserved_names` in `src/compiler/lib/typechecker.ml`.
- Bootstrap: `bi_init()` in `src/bootstrap/basaltc.basalt` — a data-driven registry built from `bi_register(name, tc_tag, flags)`.

The registry replaced a set of hardcoded name-hash checks. Tags: `BI_TC_NONE/VOID/INT/STRING/PTR_INT/PTR_VOID/MEM_ALLOC/MEM_RESIZE/MEM_FREE`; flags: `BI_FLAG_RESERVED/OWNED/CONSUME/DYNFIELD/MAIN`. Lookup is by `sym_len` + `sym_hash`, matching what the old code did, but adding a built-in is now a table entry instead of touching five functions.

**Worked example — exposing `basalt_inc_join`** (a runtime helper that was reserved but not callable):

1. Bootstrap: one line in `bi_init()`:

   ```basalt
   bi_register("basalt_inc_join", BI_TC_STRING, BI_FLAG_RESERVED);
   ```

2. Host: one line in `ast.ml`:

   ```ocaml
   ("basalt_inc_join", ([TString; TString], TString))
   ```

3. Add a fixture (`tests/regression/builtin_join_test.basalt`) and register it.

That is the whole change. Before the registry, the same addition touched the reserved list, the type-check dispatch, the emitter dispatch, and the ownership tables — five independent hardcoded hash lists that could drift apart.

**Design notes for the registry** (learned the hard way):

- `bi_init()` is **lazy**: it runs on the first `bi_lookup` during type checking. Eager initialization in `main()` fails because type-checking `bi_register`'s own body needs the registry (`grow_ints`) — a circular dependency.
- `bi_register` interns names into the **symbol-text region** (`source_len + sym_text_len`), exactly like `sym_qualified`. Writing at plain `source_len` **overwrites the qualified-name region** used by namespace symbols (e.g. `map::free`) and breaks resolution for programs longer than the registry's footprint. This was a real bug: `stdlib_containers_test.basalt` failed with `unknown function` at a line inside the map section while short programs passed.
- `bi_lookup` compares `sym_len` and `sym_hash` only — never the source text — so the comparison is immune to later buffer reuse.
- A built-in that is reserved but not callable (`printf`, `malloc`, `strlen`, ...) gets `BI_TC_NONE`. `write_int` was originally registered with `BI_TC_INT` by mistake; the old compiler treated it as reserved-only, and the Host has no `write_int` entry — so it must be `BI_TC_NONE` to preserve parity.

**Adding a *new* runtime helper** (not just exposing one) requires more: the runtime prologue is emitted independently by both compilers (`compiler.ml` prologue strings and the bootstrap's `write_string` prologue emission), and both copies must stay identical, or Host and Bootstrap binaries diverge.

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