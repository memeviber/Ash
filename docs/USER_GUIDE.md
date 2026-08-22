# Basalt User Guide

This guide introduces Basalt from the ground up: how to build it, write programs in it, use its standard library, and understand its safety model. It is written for someone who has never seen Basalt before but is comfortable with C or a statically typed language such as Rust, Go, or Java.

---

## 1. What is Basalt?

Basalt is a small, statically typed programming language that compiles to portable C11. It is designed for small systems programs, compiler implementation, and data-oriented utilities.

The project is **self-hosting** and its production workflow is deliberately **Bootstrap-only**. The active compiler is written in Basalt at `src/bootstrap/basaltc.basalt`; the checked-in C seed at `src/bootstrap/basaltc.seed.c` is the only compiler seed used to build it. The historical OCaml implementation under `src/compiler/` is frozen and MUST NOT be modified, built, or used for this workflow.

A language change is complete only when the frozen seed compiles the current Bootstrap source, the generated C compiles as strict C11, the Bootstrap-only compatibility and stress suites pass, and the fixed-point generations are byte-identical (see `docs/DEVELOPER_GUIDE.md`).

### Design philosophy

- **Compile-time safety first.** The type checker rejects invalid programs before any C is emitted. Recent additions include compile-time bounds checks for fixed arrays and a move/borrow checker for dynamic arrays.
- **No hidden runtime.** Generated programs use only standard C11 plus a small, deterministic runtime that is embedded in every emitted program.
- **Explicit over implicit.** Pointer arithmetic, `extern`, and `includec` exist and are useful, but they are deliberately low-level boundaries. Normal Basalt code works with `array`, `slice`, `map`, `option`, and `result` from the standard library.
- **Deterministic output.** The same input always produces the same C. This is what makes the fixed-point verification meaningful.

---

## 2. Building Basalt

### Requirements

- A C compiler (`gcc` recommended) supporting C11
- `bash` and the repository scripts under `scripts/`

### Build the Bootstrap compiler

All compiler artifacts belong under `.tmp/`. Build the current compiler through the frozen C seed:

```sh
source scripts/bootstrap_stage.sh
current_bin=$(bootstrap_stage "$PWD" .tmp/user-stage \\
  -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
```

The stage command first builds `src/bootstrap/basaltc.seed.c`, then uses that seed binary to translate `src/bootstrap/basaltc.basalt` into a current C compiler and builds the current compiler binary.

### Compile a Basalt program

The Bootstrap compiler accepts an explicit output path:

```sh
"$current_bin" hello.basalt .tmp/hello.c
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \\
  .tmp/hello.c -o .tmp/hello.bin
.tmp/hello.bin
```

For a stable self-hosted compiler, run `bash scripts/fixed_point.sh`; it verifies the frozen seed through successive Bootstrap generations. The OCaml files under `src/compiler/` are intentionally outside this workflow.

When a stable fixed-point candidate is ready for seed promotion, use `scripts/promote_seed.sh` instead of copying it manually. The script checks the default `.tmp/fixed-point/n3.c`/`n4.c` pair, copies the candidate into `src/bootstrap/basaltc.seed.c`, formats it with the repository `.clang-format` policy, and refreshes the production SHA-256 file. To format or check another generated C file without promotion, use `scripts/format_seed_c.sh [path]` or `scripts/format_seed_c.sh --check [path]`. These scripts require `clang-format` 18 or newer and never modify the frozen OCaml Host compiler.

The compiler accepts `--line` and `--no-line` before the input path. Mapping is enabled by default and emits C `#line` records that point diagnostics and debugger locations back to `.basalt` files. Use `--no-line` when a consumer requires generated C without source directives:

```sh
"$current_bin" --line hello.basalt .tmp/hello.c
"$current_bin" --no-line hello.basalt .tmp/hello-no-line.c
```

For a one-command build, use auto-compile mode. It keeps `#line` source mapping enabled by default, so compiler diagnostics and debugger locations refer back to the `.basalt` source. Use `--no-line` explicitly when a generated C consumer requires no source directives; `--line` may be supplied explicitly as well. It keeps compiler arguments as separate argv elements and never builds a shell command by string concatenation:

```sh
"$current_bin" --compile hello.basalt -o .tmp/hello.bin \
  --cc gcc -- -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
.tmp/hello.bin
```

`--compile` writes the intermediate C beside the requested input using the `.c` suffix unless an explicit output convention is selected by the implementation, and defaults the executable name to `.out` when `-o` is omitted. The legacy form `<input.basalt> [output.c]` remains a C-generation operation; it does not silently compile or execute the result. Arguments after `--` are passed in order before the generated C input and before the final `-o <binary>` pair. A failed compiler returns its nonzero status and forwards compiler stderr. The repository's Bootstrap and fixed-point scripts pass `--no-line` only while translating the compiler source itself, keeping the frozen seed artifact compact; this build policy does not change the compiler's default behavior for user programs.

### 2.1 Manage dependencies

The repository-side package manager is available as `python3 scripts/basalt_pkg.py`. It is intentionally separate from compiler syntax: it resolves and verifies source packages, while the Bootstrap compiler continues to resolve ordinary `include` paths relative to the including source file.

Create a manifest, add a requirement, resolve it, and inspect the resulting graph:

```sh
python3 scripts/basalt_pkg.py --root . init
python3 scripts/basalt_pkg.py --root . add text@^1.2.0
python3 scripts/basalt_pkg.py --root . --registry .tmp/registry fetch
python3 scripts/basalt_pkg.py --root . tree
python3 scripts/basalt_pkg.py --root . verify
```

`fetch` writes `Basalt.lock`, verifies each registry archive against its SHA-256 checksum, validates the archive manifest and top-level directory, and atomically materializes source under `.basalt/vendor/<name>/<version>/`. The lockfile is authoritative for later `fetch`, `verify`, and `build` operations; run `update` explicitly when newer satisfying versions should be considered.

The initial registry protocol supports a local directory or an HTTPS base with an index record and immutable archive. A local dependency can be declared without a registry:

```toml
[dependencies]
local_math = { path = "../local_math" }
```

For reproducible CI or disconnected builds, set `BASALT_HOME` to a workspace directory and use the cache-only mode:

```sh
BASALT_HOME="$PWD/.tmp/package-manager-home" \\
  python3 scripts/basalt_pkg.py --root . fetch --offline
BASALT_HOME="$PWD/.tmp/package-manager-home" \\
  python3 scripts/basalt_pkg.py --root . build --offline \\
    --compiler .tmp/bootstrap.bin --cc gcc --output .tmp/app.bin \\
    --compiler-arg=-std=c11 --compiler-arg=-Wall --compiler-arg=-Werror
```

Offline commands never contact a registry and fail if the lockfile or checksum-addressed archive is absent or corrupt. `build` uses `--compiler` for the Bootstrap compiler, `--cc` (or `CC`) for the C compiler, and accepts repeated `--compiler-arg` options; each value remains a separate compiler argv element, and package-provided scripts are never executed. Native package import syntax is deliberately deferred; consult [`PACKAGE_MANAGER.md`](PACKAGE_MANAGER.md) for the complete contract and security boundary.

---

## 3. Your first program

Basalt source files use the `.basalt` extension.

```basalt
func main(): int {
  print "hello, world";
  print 1 + 2;
  return 0;
}
```

```sh
source scripts/bootstrap_stage.sh
current_bin=$(bootstrap_stage "$PWD" .tmp/hello-stage \\
  -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
"$current_bin" hello.basalt .tmp/hello.c
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror \\
  .tmp/hello.c -o .tmp/hello.bin
.tmp/hello.bin
```

Output:

```
hello, world
3
```

`print` accepts integer and string values (and pointer values, printed with `%p`). The conventional entry point is `func main(): int { ... }`.

---

## 4. The language tour

### 4.1 Program structure

A program is a sequence of:

- `include` / `includec` directives (pull in Basalt sources or C sources)
- `extern` declarations (C functions)
- `struct` / `enum` declarations
- `namespace` blocks
- global `let` / `const` declarations
- `func` definitions

### 4.2 Lexical forms

- Line comments: `// comment`
- Integer literals: decimal (`42`)
- String literals: double quotes (`"hello"`)
- Character literals: single quotes (`'a'`), with common escapes
- String escapes include `\n`, `\t`, `\"`, `\\`, and the other usual control escapes
- `null` is the null pointer literal

### 4.3 Types

| Type | Notes |
| --- | --- |
| `int` | 32-bit integer; also used for booleans (`bool` is compatible) |
| `bool` | `true` / `false`, compatible with `int` |
| `char` | Integer-like; can be compared with `int`, widened to `int` |
| `string` | Immutable text; `const char*` in emitted C |
| `float`, `double` | Floating point |
| `void` | No value (functions, untyped pointers) |
| `T*` | Pointer to `T` (postfix star) |
| `T[n]` | Fixed array of `T` with compile-time length `n` |
| `struct` types | Nominal records, possibly generic |
| `enum` types | Named constant sets |
| `fn(..): R` | Function pointer types |

Examples:

```basalt
let x: int = 42;
let b: bool = true;
let c: char = 'A';
let s: string = "text";
let p: int* = &x;
let a: int[4] = 0;              // fixed array, zero-initialized
let f: fn(int, int): int = &add; // function pointer (see 4.9)
```

### 4.4 Operators and precedence

Arithmetic: `+`, `-`, `*`, `/`, `%` (modulo). Modulo has multiplicative precedence, exactly like `*` and `/`:

```basalt
let residue: int = (17 + 8) % 5;   // 0
```

Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`. Logical negation is the unary `!` operator and accepts integer-like scalar values, including `bool` and `char`; it produces a `bool` result.

Logical: `!`, `&&`, `||` (scalar operands). Bitwise operators for integer operands: `&` (also used for address-of), `^` (xor), `~` (not). There is no single-`|` operator and no shift operator; use `+`/`*` or comparisons instead.

Assignment: `=` (assignment expression), `&` (address-of), `*` (dereference), `[]` (indexing, see 4.7).

### 4.5 Control flow

```basalt
if x > 0 then { print "positive"; } else { print "non-positive"; }

while x > 0 { x = x - 1; }

let i: int = 0;
for (i = 0; i < 10; i = i + 1) { print i; }   // C-style for; declare the loop variable first

break;    // exit innermost loop
continue; // skip to next iteration
```

### 4.6 Functions, namespaces, and generics

```basalt
func add(a: int, b: int): int {
  return a + b;
}
```

Namespaces group declarations and are addressed with `::`:

```basalt
namespace math {
  func twice(x: int): int { return x * 2; }
}

func main(): int {
  print math::twice(21);
  return 0;
}
```

Generic functions and generic structs use angle brackets:

```basalt
func first_or<T>(a: T, b: T, fallback: T): T {
  if a == b then { return a; }
  return fallback;
}

struct Pair<A, B> {
  left: A;
  right: B;
}
```

Generic calls do not require explicit type arguments; they are inferred from the arguments (`first_or(1, 1, 0)`).

### 4.7 Closures

Closures are typed function values that may capture selected bindings. Captures are explicit, so the ownership behavior is visible at the point where the closure is created:

```basalt
func main(): int {
  let base: int = 10;
  let add_base: closure(int): int = fn[borrow base](amount: int): int {
    return base + amount;
  };
  print (add_base)(5);
  return 0;
}
```

Use `move` when the closure should own an already-owned value. The source cannot be used after the move:

```basalt
func main(): int {
  let inner: closure(): int = fn[](): int { return 9; };
  let invoke_later: closure(): int = fn[move inner](): int {
    return (inner)();
  };
  print (invoke_later)();
  return 0;
}
```

Use `borrow_mut` for an exclusive mutable capture when the closure is used within the source binding’s lifetime:

```basalt
func main(): int {
  let value: int = 1;
  let increment: closure(): int = fn[borrow_mut value](): int {
    value = value + 1;
    return value;
  };
  print (increment)();
  return 0;
}
```

A closure that borrows a block-local binding MUST NOT be returned from that block or stored where it outlives the binding. A move capture is also a compile-time ownership transfer, not a runtime copy. The Bootstrap compiler reports code **60** for a borrowed capture that escapes its source lifetime and code **61** when a moved-capture source is used again.

### Option and Result

Basalt’s standard library provides total, generic absence and error values. `option::Option<T>` uses `option::some(value)` for a present value and `option::none(zero)` for an absent value. Because generic structs are fully materialized in generated C, `none` takes an explicit zero value of `T`:

```basalt
include "../../src/stdlib/option.basalt"

let present: option::Option<int> = option::some(42);
let absent: option::Option<int> = option::none(0);
print option::value_or(present, 0);   // 42
print option::value_or(absent, 99);   // 99
```

The canonical Option helpers are `is_some`, `is_none`, `unwrap_or`, `value_or`, `map`, `map_or`, `filter`, `contains`, `or_else`, and `and_option`. They are total: an absent value never causes a panic and callbacks are not invoked for an absent branch. Existing programs may continue to use the historical `result::Option<T>` compatibility names, but new code SHOULD use the `option` namespace.

`result::Result<T, E>` represents success or failure. Both constructors initialize the inactive payload explicitly:

```basalt
include "../../src/stdlib/result.basalt"

let success: result::Result<int, string> = result::ok(7, "");
let failure: result::Result<int, string> = result::err(0, "not found");
print result::value_or(success, 0);              // 7
print result::value_or(failure, 0);              // 0
print result::error_or(failure, "no error");    // not found
```

Use `map` and `map_error` to transform one branch, `map_or` and `map_error_or` for total fallback-based transformations, and `and_result` or `or_result` to compose results without manually inspecting the tag. `is_ok` and `is_err` expose the branch state. Result and Option operations return initialized values by value, so callers should assign the updated container or value when using a mutating standard-library operation.

### 4.8 Arrays: fixed and dynamic

**Fixed arrays** have a compile-time length and are zero-initialized with `= 0`:

```basalt
let a: int[3] = 0;
a[0] = 1;
a[2] = 3;
print a[0] + a[2];   // 4
```

Indexing a fixed array with a **constant** outside its length is a compile-time error:

```basalt
let a: int[3] = 0;
print a[5];  // Type Error: array index 5 out of bounds for length 3
```

Even `-1` (including the `0 - 1` form) is caught at compile time.

**Dynamic arrays** live in the standard library as `array::Array<T>` (and `slice::Slice<T>`). They own their heap memory; see Section 7.

### 4.8 Structs and enums

```basalt
struct Point {
  x: int;
  y: int;
}

func main(): int {
  let p: Point = 0;      // zero-initialized
  p.x = 3;
  p.y = 4;
  print p.x + p.y;
  return 0;
}
```

Enums are named integer constants:

```basalt
enum Color {
  Red,
  Green,
  Blue
}

func main(): int {
  let c: Color = Green;
  print c;               // 1
  return 0;
}
```

### 4.9 Function pointers

```basalt
func add(a: int, b: int): int {
  return a + b;
}

func main(): void {
  let f: fn(int, int): int = &add;
  let x: int = f(2, 3);
  print x;               // 5
}
```

### 4.10 C interoperability

**`extern`** declares a C function that the final link step provides. The legacy form has no header metadata and remains useful when the runtime or an `includec` file already provides the declaration:

```basalt
extern func c_abs(x: int): int;

func main(): int {
  print c_abs(0 - 7);    // 7
  return 0;
}
```

For a controlled declaration, place a header path immediately after `extern`:

```basalt
extern "stdlib.h" func abs(x: int): int;

func main(): int {
  print abs(0 - 7);      // 7
  return 0;
}
```

The controlled header literal is emitted as a quoted C include. It must be non-empty and contain only letters, digits, `.`, `/`, `_`, and `-`; preprocessor syntax such as angle brackets, quotes, backslashes, and newlines is rejected. Repeated declarations using the same header emit that header only once. Controlled extern parameters and returns may use scalar types, pointers, fixed-size arrays, or named structs. Dynamic arrays, tuples, variants, generic types, and other compiler-only representations are rejected at compile time with diagnostics 55 or 56.

**`include`** pulls in another Basalt file (paths are resolved relative to the including file's directory):

```basalt
include "../../src/stdlib/option.basalt"
```

**`includec`** embeds raw C material into the generated program. It remains the escape hatch for helper implementations and declarations that are not expressed through the controlled `extern` surface:

```basalt
includec "my_helpers.c"

extern func compute(x: int): int;
```

Use controlled `extern "header.h"` declarations when the C function boundary is stable and representable by Basalt types. Use `includec` only for C text that must be injected directly; its contents are not type-checked by Basalt.

### 4.11 Built-in functions

A small set of runtime functions is reserved and callable directly:

| Built-in | Signature (informal) | Purpose |
| --- | --- | --- |
| `memory_alloc` | `(int count, T witness) -> T*` | Allocate `count` elements of the witness type |
| `memory_resize` | `(T* p, int old, int new, T witness) -> T*` | Resize a tracked allocation |
| `memory_free` | `(T* p) -> void` | Release a tracked allocation |
| `alloc_ints(n)` | `int -> int*` | Allocate `n` ints (checked) |
| `grow_ints(p, old, new)` | `int* -> int*` | Resize an int array |
| `free_ints(p)` | `int* -> void` | Release an int array |
| `open_file` / `read_char` / `close_file` | file I/O | Read a text file character by character |
| `write_char(f, c)` / `write_string(f, s)` | file I/O | Write to an open file |
| `basalt_inc_realpath(s)` / `basalt_inc_join(a, b)` | `string -> string` | Path helpers (used by `include`) |
| `basalt_include_*` | include machinery | Internal include-engine hooks |
| `basalt_sys_run` and accessors | `(executable, argv, argc, max_output)` | Internal argv-oriented process runner used by `sys` |

More than 50 names (`printf`, `malloc`, `free`, `strlen`, `exit`, ...) are **reserved**: defining a function with one of those names is rejected, because the name would collide with the C runtime.

```basalt
func printf(x: int): int { return 0; }   // Type Error: reserved runtime function name printf
```

### 4.12 Ownership, borrowing, lifetimes, and escape analysis

Basalt uses an explicit ownership checker for heap-backed values, including generic `array::Array<T>` and `slice::Slice<T>` representations. An owner is tracked independently from lexical name lookup, and ownership state is updated at declarations, assignments, returns, defers, ordinary calls, generic calls, built-ins, and indirect calls.

Function parameters can state their ownership mode:

```basalt
include "../../src/stdlib/array.basalt"

func consume(move values: array::Array<int>): void {
  values = array::free(values);
}

func inspect(borrow p: int*): int {
  return *p;
}

func update(borrow_mut p: int*): void {
  *p = 41;
}

func main(): int {
  let value: int = 7;
  {
    let view: int* = &value;
    print inspect(view);       // temporary shared borrow
  }                            // the borrow is released here
  update(&value);              // temporary exclusive borrow
  let values: array::Array<int> = array::new(2, 0);
  consume(move values);        // explicit ownership transfer
  // print array::length(values); // Type Error: use of moved value
  return value - 41;
}
```

The rules are as follows. An unannotated parameter preserves compatibility behavior and does not consume its argument. A `move` parameter requires `move expression` at the call site and makes the source unusable after the transfer. A `borrow` parameter permits read-only access for the duration of the call. A `borrow_mut` parameter permits mutation but requires an exclusive borrow; shared and mutable borrows cannot overlap. Releasing an owner, mutating it, or moving it while an incompatible borrow is active is rejected at compile time.

Borrow state is lexical. When a borrowed binding leaves its scope, the associated borrow count is unwound, so a temporary view does not leak into an outer block. Returning a pointer derived from a block-local owner is rejected as a lifetime escape. Pointers derived from globals or formal parameters are allowed under lifetime elision. Named lifetime parameters and closure-capture lifetime inference are reserved for a future language revision.

The explicit `move` expression is intentionally transparent in generated C: it changes only compile-time ownership state and emits the underlying expression once. Raw pointers, `extern`, and `includec` remain low-level boundaries; the checker validates tracked Basalt ownership at their call boundaries but cannot infer ownership behavior inside arbitrary injected C.

---

## 5. Null safety with `option`

Basalt has no `null` values and no null pointers in normal code. Absence is modeled with `option::Option<T>` from the standard library:

```basalt
include "../../src/stdlib/option.basalt"

func find_value(flag: int): option::Option<int> {
  if flag == 1 then { return option::some(42); }
  return option::none(0);
}

func main(): int {
  let present: option::Option<int> = find_value(1);
  let absent: option::Option<int> = find_value(0);

  print option::is_some(present);   // 1
  print option::is_none(absent);    // 1

  // The only way to read the payload is unwrap_or:
  print option::unwrap_or(present, 7);  // 42
  print option::unwrap_or(absent, 7);   // 7
  return 0;
}
```

`Option<T>` is a tag (`present`) plus a payload (`value`). The module deliberately provides **no** `unwrap` that could panic: the total-function API (`is_some`, `is_none`, `unwrap_or`) guarantees that reading the payload always has a defined outcome. This is Basalt's null-safety discipline: absence is explicit, checked, and cannot crash.

`result::Result<T, E>` works the same way for error handling:

```basalt
include "../../src/stdlib/result.basalt"

func divide(a: int, b: int): result::Result<int, string> {
  if b == 0 then { return result::err(0, "division by zero"); }
  return result::ok(a / b, "unused");
}

func main(): int {
  let r: result::Result<int, string> = divide(10, 2);
  print result::is_ok(r);            // 1
  print result::unwrap_or(r, 0);     // 5
  return 0;
}
```

---

## 6. The standard library

Basalt's standard library is namespace-qualified and is implemented in Basalt except for small, audited C11/POSIX/Windows boundary shims. Heap-backed values are returned by value, so a mutating operation must be assigned back to the owner. The exact ownership and error contract is maintained in [`STDLIB_API.md`](STDLIB_API.md).

| Module | File | Main capabilities |
| --- | --- | --- |
| `array` | `src/stdlib/array.basalt` | Generic owned dynamic arrays, growth, reserve, map, filter, slicing, and stable sort |
| `slice` | `src/stdlib/slice.basalt` | Generic growable slice values with indexing, map, filter, and stable sort |
| `map` | `src/stdlib/map.basalt` | Generic open-addressed `HashMap<K,V>`, custom hash/equality, growth, removal, and clear |
| `set` | `src/stdlib/set.basalt` | Generic set built on the map implementation, insertion/removal/membership, and iteration |
| `deque` | `src/stdlib/deque.basalt` | Generic ring-buffer double-ended queue; `push_front`, `push_back`, and ownership-safe pop results |
| `iter` | `src/stdlib/iter.basalt` | Iterators and `for_each`/`any`/`all`/`fold` helpers for arrays, slices, maps, sets, and deques |
| `option` | `src/stdlib/option.basalt` | Total `Option<T>` operations such as `some`, `none`, `map`, `filter`, and fallback access |
| `result` | `src/stdlib/result.basalt` | Generic `Result<T,E>` construction, inspection, mapping, composition, and fallback access |
| `string` | `src/stdlib/string.basalt` | Byte-oriented UTF-8 validation, search, substring, trim, replacement, split, and `StringView` |
| `string_builder` | `src/stdlib/string_builder.basalt` | Owned append buffer and ownership-safe `finish` returning value plus reset builder |
| `path` | `src/stdlib/path.basalt` | Platform separator, root-aware join, normalization, basename, and extension |
| `filesystem` | `src/stdlib/filesystem.basalt` | Result-based text file open/read/write/close, metadata, directory listing, and cleanup |
| `time` | `src/stdlib/time.basalt` | Monotonic and wall clocks, durations, elapsed checks, and sleep |
| `process` | `src/stdlib/process.basalt` | Environment, working directory, bounded stdin, argv-safe process handles, wait, timeout, and signal |
| `concurrency` | `src/stdlib/concurrency.basalt` | Atomics, bounded channels, threads, mutexes, and cooperative cancellation |
| `format` | `src/stdlib/format.basalt` | Typed fixed-format integer, character, floating-point, and string appends |
| `random` | `src/stdlib/random.basalt` | Deterministic SplitMix64 PRNG, bounded sampling, floating output, and OS entropy |
| `io` | `src/stdlib/io.basalt` | Standard input/output helpers, including line and integer input |
| `sys` | `src/stdlib/sys.basalt` | Structured argv-oriented process execution and bounded stdout/stderr capture |

### 6.1 Generic collections and iterators

`array::Array<T>`, `slice::Slice<T>`, `map::HashMap<K,V>`, `set::Set<K>`, and `deque::Deque<T>` support typed values rather than an int-only special case. Arrays, slices, maps, and deques grow through their own `reserve`/`ensure_capacity` policy; callers do not call a separate raw memory module. Stable sorting takes a typed comparison function and preserves the relative order of equivalent elements.

Because Basalt passes structs by value, operations that change ownership or cursor state return the updated value. A deque pop therefore returns `deque::PopResult<T>`, containing both `deque` and `option::Option<T>`:

```basalt
include "../../src/stdlib/deque.basalt"
include "../../src/stdlib/option.basalt"

func main(): int {
  let queue: deque::Deque<int> = deque::new(2, 0);
  queue = deque::push_back(queue, 10);
  queue = deque::push_back(queue, 20);
  let popped: deque::PopResult<int> = deque::pop_front(queue);
  queue = popped.deque;
  print option::unwrap_or(popped.value, 0); // 10
  queue = deque::free(queue);
  return 0;
}
```

An iterator has the same explicit progression rule. Assign `step.iterator` after every `next` call; an exhausted iterator returns an absent option. Map iteration visits occupied buckets, while set iteration uses the set's underlying key/state arrays without copying or freeing the collection.

### 6.2 Strings, views, builders, and paths

The `string` module has a deliberately explicit UTF-8 model. `byte_len` and its compatibility alias `len` count encoded bytes up to the terminating NUL; `byte_at` accepts a byte offset and returns one value in `0..255`. These functions do not count or decode Unicode code points. `StringView` also uses byte offsets and byte lengths. Consequently, a multi-byte character occupies several byte positions, and indexing the middle of its encoding is legal only as a byte operation, not as a decoded character operation.

The UTF-8-aware functions operate on decoded scalar values. `utf8_validate(s)` rejects malformed, truncated, overlong, surrogate, and out-of-range encodings. `codepoint_len(s)` returns the number of decoded code points or `-1` if validation fails. `codepoint_byte_offset(s, index)` converts a code-point index to its byte offset, and `codepoint_at(s, index)` returns the decoded scalar value; both return `-1` for invalid input or an invalid index. They never silently treat a byte offset as a code-point index.

For sequential traversal, use the borrowed `Utf8Iterator` protocol:

```basalt
include "../../src/stdlib/string.basalt"

func main(): int {
  let text: string = "Aé€😀";
  let cursor: str::Utf8Iterator = str::utf8_iter(text);
  let step: str::Utf8NextResult = str::utf8_iter_next(cursor);
  if step.status == 0 then { print step.codepoint; }
  cursor = step.iterator;
  // status 0 = yielded a code point, 1 = end, 2 = malformed UTF-8.
  return 0;
}
```

`utf8_iter_next` advances its cursor by the encoded width of the yielded code point. End-of-input and malformed input are distinct: end has status `1` and keeps the cursor at the end, while malformed input has status `2` and does not advance it. `Utf8Iterator` is borrowed; it owns neither the source nor a copy of its bytes. It must not outlive the source string or be used after the source is released or mutated.

A view is likewise non-owning and is represented as `{source, offset, len}`, not as an independently allocated pointer. `view_to_string`, `substring`, `trim`, `replace`, `split`, and `concat` return owned strings or owned string arrays and must be released with the matching helper. Basalt strings are NUL-terminated, so they are not a binary-buffer abstraction and cannot faithfully carry an embedded NUL byte.

`string_builder::finish` and `format::finish` return a result struct containing the produced owned string and a reset builder/formatter. The caller must assign the returned builder field before freeing it. This prevents a by-value struct copy from leaving two owners for the same buffer.

`path::join` is root-aware, `normalize` removes `.` and resolves lexical `..` components while preserving the appropriate root/relative form, and `basename`/`extension` use both the platform separator and the accepted alternate separator. Path functions return owned strings. Filesystem reads are intentionally text-oriented: the result is NUL-terminated and is not a binary buffer API, so arbitrary embedded NUL bytes are not faithfully represented.

### 6.3 Filesystem and time

Filesystem functions return `result::Result<..., int>`. The public error categories include `1` for invalid argument or handle, `2` for a missing path, `3` for permission failure, and `5` for directory end-of-stream. `filesystem::directory` returns an owned `array::Array<string>`; release every entry with `filesystem::free_entries`, including an error path's partially built list.

`time::monotonic_ns` is suitable for elapsed measurements and `time::wall_seconds` is a wall-clock timestamp. `time::duration_ms` validates nonnegative durations, `time::elapsed` compares a start timestamp with a duration, and `time::sleep_ms` may return an operating-system error. These APIs do not treat wall-clock adjustment as a valid monotonic timeout source.

### 6.4 Process and concurrency boundaries

The `process` module passes executable names and argument arrays directly to the operating system; it does not construct a shell command. On POSIX, an exec-error pipe reports lookup/setup failures to the parent before a process handle is returned. A successful handle must be reaped with `wait` or a supported wait operation. `process::free` rejects an un-awaited handle with error `6` rather than silently creating a zombie. Timeout and signal support return the documented unsupported error `8` on Windows where the implementation cannot provide the same capability.

Environment and working-directory strings returned by `process::getenv` and `process::cwd` are owned copies. `stdin_line` is bounded by its requested length. Callers must inspect every `Result` and release successful owned strings.

Concurrency handles are opaque runtime resources. Atomics provide load/store/fetch-add/compare-exchange, channels are bounded and closeable, threads must be joined, mutexes must be unlocked before release, and cancellation is cooperative: a worker must poll `cancelled(handle)`. Invalid handles return an error status where the API exposes one; no operation makes an invalid opaque pointer valid.

### 6.5 Formatting and randomness

`format` accepts typed values through separate functions rather than accepting a user-controlled C `printf` format string. `append_int`, `append_i64`, `append_f64`, `append_char`, and `append_string` use fixed internal conversion formats. The formatter's result is an owned string and follows the same reset-builder ownership rule described above.

`random::seed` creates a deterministic SplitMix64 generator. Equal seeds produce equal sequences on the same Basalt ABI, and `next_bounded` rejects a zero bound. `random::try_seed` exposes allocation failure as a `Result`; `entropy_u64` uses the operating-system source where available and reports an error rather than claiming cryptographic guarantees for an unavailable platform source. The deterministic generator is reproducible, not a substitute for cryptographic randomness.

All modules are **generic** and namespace-qualified. A minimal map example is:

```basalt
include "../../src/stdlib/map.basalt"

func main(): int {
  let m: map::HashMap<int, int> = map::new(0, 0);
  m = map::put(m, 7, 70);
  m = map::put(m, 9, 90);
  print map::get_or(m, 7, 0 - 1);   // 70
  print map::get_or(m, 8, 0 - 1);   // -1
  m = map::free(m);
  return 0;
}
```

### Process execution with `sys`

The `sys` module provides a structured process API for tools and build helpers. Include it together with `array`:

```basalt
include "../../src/stdlib/array.basalt"
include "../../src/stdlib/sys.basalt"

func main(): int {
  let args: array::Array<string> = array::new(0, "");
  let result: sys::Output = sys::run("printf", args, 4096);
  print result.status;
  print result.stdout;
  print result.stderr;
  print result.truncated;
  print result.spawn_error;
  return 0;
}
```

Each array item is passed as one argv item. An argument such as `two words` or `a"b` is not reparsed by a shell, so structured execution is the safe default for user-controlled data. `status` is zero on success, nonnegative for a normal child exit, and negative for signal termination. `spawn_error` is positive only when process setup or executable lookup fails, and `succeeded` is true only when both status and spawn_error indicate success.

The `max_output` limit applies independently to stdout and stderr. When a child writes more than the limit, the captured value is the prefix and `truncated` is set, while the runner continues draining both pipes and waits for the child so a full pipe cannot deadlock. Negative or excessively large limits are rejected. POSIX uses fork/exec and Windows uses `CreateProcessW`; on Windows, UTF-8 arguments are converted to UTF-16 and individually quoted into a direct command line, without invoking `cmd.exe`. Both targets expose the same structured output fields and capture stdout and stderr through separate concurrently drained pipes. There is deliberately no implicit shell API. Shell syntax, if required, must be an explicit and audited `includec`/FFI boundary.

Direct string indexing is byte indexing. For ASCII text this has the expected character-like result, but it must not be used to traverse a multi-byte UTF-8 encoding:

```basalt
let s: string = "hello";
print s[0] == 'h';        // 1
let byte_value: int = s[1]; // byte value: 101
print byte_value;            // 101
```

For non-ASCII text, use `utf8_validate`, `codepoint_at`, `codepoint_byte_offset`, or `Utf8Iterator` as described in Section 6.2.

---

## 7. Reading diagnostics

The Host compiler reports `Type Error: <message>` and a source position. Common messages:

| Message | Meaning |
| --- | --- |
| `array index K out of bounds for length N` | Constant index outside a fixed array |
| `reserved runtime function name X` | `X` collides with a C runtime function |
| `use of moved value X` | You read a value after moving it |
| `cannot mutate borrowed value: X` | A borrow of `X` is still live |
| `cannot release borrowed value: X` | You freed a value while it was borrowed |
| `double release of moved value X` | You released the same owner twice |
| `borrow escapes function through return: X` | A borrow of a local left the function |
| `initializer type mismatch for X` | Type mismatch in a `let` |
| `argument type mismatch in F` | Wrong argument type in a call |

The Bootstrap compiler prints `type error: <code>` (with a message for most codes) using the same numbering, so both compilers agree on what is invalid.

At runtime, tracked-allocation errors (out of bounds, double release, registry violations) terminate with **exit code 2** and a message on stderr — deterministic and fail-closed.

---

## 8. Portability and safety checklist

Generated programs are C11 and are expected to compile cleanly under:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

They are additionally exercised under AddressSanitizer and UndefinedBehaviorSanitizer by the project's test suites. The golden rules:

1. Normal code: use `array`, `slice`, `map`, `option`, `result` — never raw heap pointers.
2. If you need raw pointers, follow the ownership rules and test with sanitizers.
3. Never define functions whose names collide with the runtime (reserved names are rejected anyway).
4. Fixed arrays: use constant indexes, or keep them in bounds — the compiler checks constants for you.
5. For process execution, use `sys::run` with an argv array; do not interpolate untrusted text into a shell command.
6. Bound captured output with `max_output`, inspect `spawn_error`, and treat `truncated` as meaningful data rather than an I/O success indicator.
7. Keep generated C, binaries, and auto-compile intermediates under `.tmp/` during Bootstrap development.

---

## 9. Where to go next

- `docs/LANGUAGE_SPEC.md` — the formal language specification
- `docs/DESIGN.md` — architecture of the Host/Bootstrap pair
- `docs/DEVELOPER_GUIDE.md` — how to modify the compilers, add builtins and stdlib modules, and verify with the fixed-point and suite machinery
- `docs/RELEASE_NOTES.md` — release history
- `tests/regression/` — working example programs for nearly every feature