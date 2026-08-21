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

| Module | File | Contents |
| --- | --- | --- |
| `array` | `src/stdlib/array.basalt` | Owned dynamic arrays: `new`, `push`, `get`, `set`, `len`, `free` |
| `slice` | `src/stdlib/slice.basalt` | Lightweight slices over arrays |
| `map` | `src/stdlib/map.basalt` | Hash map: `new`, `put`, `get_or`, `remove`, `free` |
| `option` | `src/stdlib/option.basalt` | `Option<T>`: `some`, `none`, `is_some`, `is_none`, `unwrap_or` |
| `result` | `src/stdlib/result.basalt` | `Result<T, E>`: `ok`, `err`, `is_ok`, `is_err`, `unwrap_or`, `error_or` |
| `string` | `src/stdlib/string.basalt` | `byte_len`, `byte_at`, `eq`, and string helpers |

All modules are **generic** and namespace-qualified:

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

Strings support character indexing and comparison:

```basalt
let s: string = "hello";
print s[0] == 'h';        // 1
let code: int = s[1];     // char widens to int: 101
print code;               // 101
```

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

---

## 9. Where to go next

- `docs/LANGUAGE_SPEC.md` — the formal language specification
- `docs/DESIGN.md` — architecture of the Host/Bootstrap pair
- `docs/DEVELOPER_GUIDE.md` — how to modify the compilers, add builtins and stdlib modules, and verify with the fixed-point and suite machinery
- `docs/RELEASE_NOTES.md` — release history
- `tests/regression/` — working example programs for nearly every feature