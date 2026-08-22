# Basalt Language Specification

## Status and conformance

This document is the normative language contract for the Bootstrap Basalt compiler. The implementation of record is `src/bootstrap/basaltc.basalt` together with the checked-in C seed `src/bootstrap/basaltc.seed.c`; the frozen seed must be able to compile the Bootstrap source without invoking the Host compiler. A language change is conforming only when the Bootstrap compiler accepts every valid corpus case, rejects every invalid corpus case before C emission, produces strict-C11 output, and preserves the fixed-point property described below.

The specification uses **MUST** for a required rule, **MUST NOT** for a forbidden behavior, and **MAY** for an implementation extension that does not change the defined behavior of conforming programs.

## Program model

A Basalt source file is a sequence of declarations. Declarations may be type declarations, namespace declarations, `include` or `includec` directives, global bindings, external declarations, and function definitions. The conventional executable entry point is:

```basalt
func main(): int {
  return 0;
}
```

Statements are terminated by semicolons. Blocks use braces. A conditional has the form `if condition then { ... } else { ... }`; `while condition { ... }` is the primitive loop form. `for` is accepted by the compiler and lowered to equivalent C control flow. `break` and `continue` apply to the innermost active loop. `defer` schedules a statement for execution when the enclosing function or scope exits according to the ownership rules.

The compiler MUST evaluate a source expression into an AST before type checking and C emission. It MUST NOT use C emission as a substitute for source-level type validation.

## Lexical rules

Identifiers contain letters, decimal digits, and underscores, and the first character MUST NOT be a digit. Keywords are reserved when they appear as complete identifiers. Comments begin with `//` and continue to the end of the line.

Integer literals are decimal. The compiler MUST detect overflow before accumulation and MUST reject a literal that cannot be represented by its contextual integer type. Decimal floating literals default to `f64`/C `double`; a literal in an `f32` context is emitted with an `f` suffix. String literals use double quotes. Character literals use single quotes and support escaped quotes, backslashes, and the documented control escapes.

## Types

The primitive and derived types currently covered by the Bootstrap contract are:

| Basalt type | C representation | Contract |
| --- | --- | --- |
| `int` | `int` | The default signed integer type. |
| `bool` | `int`-compatible Boolean representation | Used by conditions and logical operations. |
| `char` | `unsigned char`-compatible character value | Character values are byte-oriented and range from 0 through 255. |
| `string` | Basalt-managed string representation | String operations are byte-oriented, not Unicode code-point operations. |
| `f32` / `float` | `float` | Single-precision floating type. |
| `f64` / `double` | `double` | Double-precision floating type. |
| `void` | `void` | Function return type with no value. |
| `T*` | Pointer to the C representation of `T` | Pointers use postfix `*`. |
| `T[N]` | Fixed C array | The length is part of the type and is bounds-checked for literal indices. |
| `array::Array<T>` | Generic dynamic-array representation | Growth, indexing, and ownership are implemented by the standard library. |
| Named struct or enum | Generated named C type | Namespaces and generic specializations are mangled deterministically. |
| Function pointer | C function-pointer type | The parameter count and parameter types are checked before invocation. |

`f32` and `float` are aliases for the same internal kind. `f64` and `double` are aliases for the same internal kind. The two floating kinds are distinct for contextual literal emission, arithmetic result typing, generic element matching, and explicitly typed function arguments. A named `f64` expression MUST NOT be silently retyped as `f32` merely because it is passed to an `f32` parameter.

`int` and `bool` follow the current C-oriented compatibility model. Character values are integer-like where the type checker explicitly permits numeric conversion. Pointer compatibility is C-like only for the conversions implemented by the Bootstrap type checker; an arbitrary integer MUST NOT be treated as a pointer except for the documented null representation and accepted zero compatibility rule.

## Expressions and evaluation

The precedence levels below are ordered from lowest to highest. Operators on the same level associate left-to-right unless the parser defines a specific unary or assignment rule.

| Level | Operators | Meaning |
| --- | --- | --- |
| 1 | `||` | Logical OR with short-circuit behavior. |
| 2 | `&&` | Logical AND with short-circuit behavior. |
| 3 | `==`, `!=`, `<`, `>` | Comparison. |
| 4 | `++` | String concatenation. |
| 5 | `\|` | Bitwise OR. |
| 6 | `^` | Bitwise XOR. |
| 7 | `&` | Bitwise AND, or address-of in unary position. |
| 8 | `<<`, `>>` | Bit shifts. |
| 9 | `+`, `-` | Addition and subtraction. Unary `-` is also supported. |
| 10 | `*`, `/`, `%` | Multiplication, division, and integer remainder. |

The parser MUST build `a + b % c` as `a + (b % c)`. The modulo operator is available only for integer-compatible operands. String concatenation MUST release intermediate managed storage according to the runtime ownership policy.

Compound assignment is a distinct semantic operation. For `lhs += rhs`, `lhs` MUST be evaluated exactly once, then updated with the compound operator. The compiler MUST NOT lower it to a duplicated `lhs = lhs + rhs` expression when `lhs` can contain an index, field access, pointer dereference, or side effect.

Logical operators MUST short-circuit. The right-hand operand of `a && b` is evaluated only when required by `a`; the right-hand operand of `a || b` is evaluated only when required by `a`.

## Declarations, scopes, and ownership

A mutable local binding uses `let name: Type = expression;`. A `const` binding is read-only after initialization. Assignments, field assignments, indexed assignments, returns, calls, variant payloads, and generic substitutions MUST be checked before C emission.

A block introduces a scope. Local bindings declared in an inner scope MUST NOT remain visible after the scope closes. The type checker tracks ownership state separately from lexical name lookup. A moved owner MUST NOT be used again. Generic owner representations such as `array::Array<T>` are tracked in the same way as dynamic arrays.

Function parameters MAY declare an ownership mode before the parameter name:

```basalt
func consume(move values: array::Array<int>): void { }
func inspect(borrow p: int*): int { return *p; }
func update(borrow_mut p: int*): void { *p = 1; }
```

An unannotated parameter has compatibility mode and does not consume its argument. A `move` parameter requires an explicit `move expression` at the call site and transfers ownership exactly once. A `borrow` parameter creates a temporary shared borrow for the duration of the call. A `borrow_mut` parameter creates a temporary exclusive borrow and permits mutation through that parameter. Shared and mutable borrows MUST conflict with one another, and mutation, move, or release of a value MUST be rejected while an incompatible borrow is active.

The `move expression` has the form `move expression`. It is valid only for an owned binding or an owned container result. After a successful move, the source binding is unusable until it is reinitialized. The checker applies these transitions at all ordinary, generic, built-in, and indirect call sites rather than only to expression statements.

A borrowed value MUST NOT outlive its owner. Returning a pointer derived from a block-local owner is a lifetime error; returning a pointer derived from a global or formal parameter is permitted under lifetime elision. Borrow metadata is unwound when the lexical binding leaves scope, so a borrow held only by an inner binding does not leak into an outer scope. Closure captures use the same ownership state: a borrowed capture is valid only while its source binding remains in lexical scope, and a moved capture makes the source unusable after the capture.

`defer statement;` registers cleanup in the active scope. Deferred operations MUST execute in reverse registration order on the corresponding exit path, including explicit returns covered by the implementation. A resource transferred out of a scope MUST NOT also be freed by the originating scope.

The `null` literal represents a null pointer value. A raw zero MAY be accepted in pointer contexts covered by the C-oriented compatibility rules, but `null` is the portable spelling for new code.

## Closures

A closure literal has the form `fn[captures](parameters): ReturnType { body }`. The capture list is explicit and each capture MUST specify one of the following modes:

| Capture mode | Meaning | Lifetime and ownership rule |
| --- | --- | --- |
| `borrow x` | Shared borrow of `x`. | The closure value MUST NOT escape the lexical lifetime of `x`; shared and mutable borrows conflict. |
| `borrow_mut x` | Exclusive mutable borrow of `x`. | The closure value MUST NOT escape the lexical lifetime of `x`; no competing borrow or mutation is permitted. |
| `move x` | Transfer ownership of `x` into the closure environment. | `x` MUST be owned and MUST NOT be used after the capture. |

A closure type annotation has the form `closure(parameters): ReturnType`. Closure types are structurally compared by parameter types and return type. A closure value is distinct from a plain function pointer even when its visible signature is the same, because it carries an environment.

Calling a closure uses the ordinary postfix-call syntax, for example `(increment)(4)`. The generated C representation is a deterministic fat value containing an environment pointer and an invoke-function pointer. The environment stores captured values, while the invoke function receives the environment pointer before the declared closure parameters. Capture-free closures still use the same representation with an empty environment marker, which keeps the ABI uniform.

A closure literal MUST be type-checked before C emission. Its parameters form an inner scope, captured names are available in the body, and the body MUST return a value compatible with the declared return type. Returning a closure that borrows a block-local binding is rejected with diagnostic code **60**. Using a binding after moving it into a closure is rejected with diagnostic code **61**. These checks apply to direct closure literals and closure values stored in local bindings.

## Console output and diagnostics

`print expression;` writes the value without appending a line terminator. `println expression;` writes the value followed by exactly one line-feed. Both statements accept the same scalar, string, character, floating-point, integer-width, and pointer categories supported by the C emitter. The left-to-right evaluation order of the expression is unchanged.

Compiler diagnostics are emitted as a readable multi-line record. The human-readable message is on the first line, and every stable field keeps its label and value on the same line, for example `diagnostic.code=`, `diagnostic.file=`, `diagnostic.line=`, `diagnostic.column=`, `diagnostic.hint=`, and `diagnostic.excerpt=`. Type-mismatch diagnostics additionally include `diagnostic.expected=` and `diagnostic.found=`. The excerpt is the final field so source text containing line breaks or delimiters remains intact.

## Option, Result, and standard-library stability

`option::Option<T>` is the canonical absence type. It is a fully materialized tagged value with a `present` flag and an initialized `value` field. `option::some(value)` constructs a present value, while `option::none(zero)` constructs an absent value whose payload is initialized to the caller-provided generic zero. The zero argument is required because the generated C representation does not use implicit uninitialized payloads.

The canonical Option API is total and MUST NOT panic or read an absent payload. It includes `is_some`, `is_none`, `unwrap_or`, `value_or`, `map`, `map_or`, `filter`, `contains`, `or_else`, and `and_option`. Callback combinators MUST preserve the input state and return a correctly initialized `none` value when the input is absent.

`result::Result<T, E>` is the canonical structured-error type. `result::ok(value, error_zero)` constructs success and initializes the error payload; `result::err(value_zero, error)` constructs failure and initializes the value payload. The total API includes `is_ok`, `is_err`, `unwrap_or`, `value_or`, `error_or`, `map`, `map_or`, `map_error`, `map_error_or`, `contains`, `and_result`, and `or_result`. Result combinators MUST preserve the inactive payload and its initialized zero value while transforming only the active branch.

The deprecated `result::Option<T>` type and its legacy helpers are removed. A reference to that namespace is a compile-time error; migration requires including `option.basalt` and using `option::Option<T>` with the canonical `option::some`, `option::none`, `option::is_some`, `option::is_none`, `option::unwrap_or`, `option::value_or`, `option::map`, `option::map_or`, `option::filter`, `option::contains`, `option::or_else`, and `option::and_option` API.

For standard-library APIs whose error type is `int`, `error::success` (`0`) is the only success value. Every failure is represented by a nonzero, module-documented category. The `value` field of a failed Result and the `error` field of a successful Result are initialized witnesses and MUST NOT be interpreted as active data. Expected absence belongs in `Option`, while operational failure belongs in `Result`; fallback sentinels are reserved for total search/accessor APIs whose contract explicitly documents them.

Standard-library containers use generic, typed storage and return updated owning values from mutating operations. `array`, `slice`, `map`, and `string_builder` preserve their existing `length` spellings and additionally provide stable `len` aliases. Container accessors with a fallback (`get_or`, `last_or`, and the map `get` value-zero form) are total. `map` also exposes `is_empty` and load-state `is_full`; `slice` provides generic `map` and `filter`; `str` provides `len` and `equals` aliases while retaining the explicit byte-oriented and UTF-8-aware names. Cleanup functions return a zeroed container value after releasing backing storage.

## Functions, generics, namespaces, and containers

A function declaration has the form:

```basalt
func remainder(a: int, b: int): int {
  return a % b;
}
```

Function calls MUST match arity and parameter types. Function pointers carry their full parameter and return signature. Calling a non-function value, using the wrong arity, or passing an incompatible argument is a compile-time error.

Generic functions and structs use type parameters and are monomorphized for C emission. A generic binding MUST be consistent across all occurrences of the type parameter. Nested generic field types MUST be recursively collected and specialized before the parent definition is emitted. A failed generic match MUST stop compilation; it MUST NOT be converted into an unconditional acceptance path.

Namespaces introduce a separate qualified scope. `namespace result { ... }` declarations are referenced through qualified names such as `result::ok(...)`. Generated C names use deterministic `__` separators for namespace segments and generic specialization arguments. Distinct source declarations that collide after mangling MUST be rejected before C emission.

Dynamic containers are generic. Their growth policy, element layout, indexing, callbacks, and cleanup behavior MUST be type-checked using the instantiated element type. `f32` and `f64` array specializations MUST remain distinct.

## Structs, enums, tuples, and pattern matching

Struct fields are declared with typed members:

```basalt
struct Point {
  x: int;
  y: int;
}
```

A struct field access MUST name an existing field, and a field assignment MUST match that field's type. Recursive structs are permitted through pointers but not through an unbounded by-value cycle.

Enums MAY be plain or tagged. Plain enum matches compare the enum value against qualified enumerators. Tagged-union matches dispatch on the tag and validate payload arity and payload types. A match over a non-exhaustive enum is a compile-time error under the current checked mode.

Tuple expressions and multiple return values use generated C struct-like representations. Tuple binding count MUST equal the number of returned values, and each element MUST be type-compatible with its binding.

## Includes and controlled C interoperability

`include "module.basalt";` loads Basalt source. A normal relative path is resolved relative to the file containing the directive. Basalt additionally defines two explicit project-root prefixes:

| Prefix | Resolution | Required layout |
| --- | --- | --- |
| `@stdlib/<path>` | `<project-root>/src/stdlib/<path>` | The standard-library source tree. |
| `@lib/<name>/<version>/<path>` | `<project-root>/.basalt/vendor/<name>/<version>/<path>` | A package materialized by the package manager. |

The project root is the compiler process working directory captured when translation starts. A caller SHOULD invoke the compiler from the project root; changing directories while nested modules load MUST NOT change prefix resolution. Prefix paths are aliases only: after expansion, the loader MUST apply the same canonicalization, loaded-module deduplication, dependency-edge insertion, active-cycle detection, and diagnostics as for a normal include. A prefix path containing a `..` segment is malformed and MUST be rejected as diagnostic **64** before path canonicalization, so an alias cannot escape the project-root or vendor namespace. The version segment in `@lib` is explicit and the compiler MUST NOT select another installed version implicitly. Both prefixes work for `include` and `includec` path opening, while raw C contents remain governed by the `includec` rules below.

The loader MUST canonicalize the candidate path before registering it in the dependency graph. A canonical path is the runtime-resolved absolute path (`realpath` on POSIX and `_fullpath` on Windows when available); the fallback path remains deterministic when canonicalization fails. The compiler records one directed edge from the including source to the canonical target, deduplicates repeated edges, rejects an active back-edge as diagnostic **62**, and treats a previously loaded canonical module as a successful no-op. An unopenable target is diagnostic **63**, and a malformed include directive is diagnostic **64**. Import diagnostics MUST expose the canonical target through `diagnostic.target` and report the included source location when available. Legacy relative include paths remain valid and are not rewritten by prefix support.

Namespaces are lexical scopes, not suffix-based global aliases. An unqualified type, function, or value name resolves in this order: the innermost lexical scope, the current namespace, each enclosing namespace from inner to outer, and finally the root namespace. A declaration in an unrelated sibling namespace MUST NOT become visible merely because its final segment matches. Qualified names are resolved by their complete namespace path. Generated C symbols remain deterministically mangled, and distinct source declarations that collide after mangling MUST be rejected before C emission.

`includec "file.c";` injects raw C material through the emitter. It is an explicit escape hatch and MUST remain isolated from ordinary Basalt type checking. The injected material is responsible for providing any implementation and any declarations that are not represented by a controlled `extern` declaration. The compiler MUST NOT infer ownership, calling convention, or type safety from raw `includec` text.

An `extern` declaration declares a C-provided function with a Basalt signature. The legacy form remains valid:

```basalt
extern func c_abs(x: int): int;
```

A controlled declaration MAY attach one validated header path immediately after `extern`:

```basalt
extern "stdlib.h" func abs(x: int): int;
```

The header literal MUST be non-empty and contain only ASCII letters, digits, `.`, `/`, `_`, and `-`. The Bootstrap emitter emits each distinct controlled header once as a quoted `#include` after the runtime prologue and before raw `includec` material. Angle-bracket spelling, quotes, backslashes, newlines, and other preprocessor syntax are not accepted in the literal. Header validation is path-syntax validation only; the selected C compiler remains responsible for locating and parsing the header.

The controlled FFI supports exactly the platform C ABI and the compiler's default calling convention. There is currently no Basalt syntax for selecting `stdcall`, `fastcall`, `vectorcall`, `sysv`, or another convention. Such annotations are not silently accepted: they are outside the grammar and MUST be reported as a parser error rather than emitted with an assumed convention. The compiler checks the Basalt-side declaration and generated C spelling, but it cannot prove that a C library's symbol, prototype, packing, or platform ABI agrees with the declaration; projects MUST compile and link against the real header and implementation as part of their platform validation.

The ABI-safe scalar mapping is:

| Basalt type | Controlled C spelling | Boundary rule |
| --- | --- | --- |
| `int` | `int` | By value. |
| `bool` | `bool` | Uses the generated runtime Boolean spelling. |
| `char` | `char` | Byte-oriented character value. |
| `f32` / `float` | `float` | By value. |
| `f64` / `double` | `double` | By value. |
| `long` | `long` | Platform C `long`; width is platform-defined. |
| `long long` | `long long` | At least the C minimum width. |
| `u8`, `u16`, `u32`, `u64` | `u8`, `u16`, `u32`, `u64` | Generated runtime typedefs provide the exact-width aliases. |
| `i8`, `i16`, `i32`, `i64` | `i8`, `i16`, `i32`, `i64` | Generated runtime typedefs provide the exact-width aliases. |
| `usize` | `usize` | Unsigned size-compatible runtime typedef. |
| `string` parameter | `const char *` | Borrowed for the duration of the call; NUL-terminated. |
| `string` return | `char *` | Borrowed C-owned or static storage; never Basalt-owned. |
| `T*` | Pointer to the mapped C type | The pointee type is checked recursively. |
| `T[N]` | C fixed array spelling | The element type is checked recursively; use is still subject to C parameter adjustment. |
| plain named struct | Generated named C struct | Non-generic fields are checked recursively and passed by value. |
| plain enum | Generated named C enum | Payload-free enumerators only. |
| `void` | `void` | Return type only. |

The checker MUST recursively reject dynamic arrays, generic or type-parameter nodes, tuples, closures, function types, tagged variants, and any named struct whose fields contain one of those representations. A named struct is accepted only after declaration identity is resolved and its traversal is guarded against recursive pointer references; an unbounded by-value recursive layout is rejected. Named enums are accepted only when they are plain payload-free enums under the generated C representation. Fixed arrays and pointers do not make an unsafe nested element safe.

Extern parameter modes are deliberately strict. `move` is rejected at an extern boundary because no portable C transfer contract exists. Unannotated scalar, pointer, fixed-array, and string parameters use compatibility mode; raw pointer and string inputs are borrowed for the call and are not implicitly freed by the callee. `borrow` and `borrow_mut` are accepted only for pointer-compatible or string parameters and retain the ordinary tracked-source conflict and lifetime checks. An explicit ownership transfer must be implemented by a future ABI contract rather than inferred from a C prototype.

Pointer and string returns from extern are non-owning borrowed values. The checker propagates this provenance through local and `const` bindings and rejects passing such a value to `memory_free`; the C library remains responsible for its lifetime. Basalt code MUST copy the bytes into a Basalt-owned string or buffer before retaining them beyond the C contract. No owned pointer/string return syntax is currently supported.

Unsafe extern parameter types use diagnostic code **55**, unsafe return types use **56**, invalid controlled header paths use **57**, an unsupported extern ownership mode uses **65**, an invalid borrow mode/type combination uses **66**, and releasing a non-owning extern pointer/string uses **67**. These diagnostics are emitted before C generation.

## Diagnostics contract

Every compile-time failure MUST produce a nonzero exit status and MUST NOT leave a C artifact at the requested output path. The Bootstrap formatter emits a concise message followed by stable fields: `diagnostic.code`, `diagnostic.file`, one-based `diagnostic.line`, one-based `diagnostic.column`, `diagnostic.excerpt`, and `diagnostic.hint`. The file and excerpt are derived from the source byte span associated with the first failure; included files MUST report their own canonicalized source path.

For type mismatch codes 12, 20, 21, 23, and 36, the formatter MUST additionally emit `diagnostic.expected` and `diagnostic.found` with human-readable type names. Ownership and lifetime violations use stable codes 33 (use after move), 35 (release requires an owned value), 37 (borrow or mutation conflict), 38 (borrowed value escapes its lifetime), 40 (owned parameter requires an explicit move), 58 (explicit move requires an owned value), 59 (borrow parameter source cannot be tracked), 60 (closure capture escapes its source lifetime), and 61 (closure moved-capture used after move). The compiler MUST preserve the first failing span while type checking so later traversal cannot overwrite the originating location. Diagnostic wording MAY evolve, but stable codes, field labels, rejection behavior, and source-location semantics are compatibility requirements. Tests MUST assert semantic rejection and the documented structured fields rather than depending only on incidental prose.

## Generated C and platform contract

The emitter targets C11 and MUST compile under:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

The runtime prologue MUST emit common headers exactly once. On native C11-thread platforms it MAY include `<threads.h>`. For MinGW/UCRT64 targets, when `_WIN32` and `__MINGW32__` are defined, the emitter MUST generate the pthread compatibility shim and include `<pthread.h>` and `<sched.h>`. The shim MUST be overridable by `BASALT_USE_NATIVE_C11_THREADS` and MUST use a type-safe join context rather than casting an `int *` to `void **`.

The aligned allocation declaration is emitted behind a guard when required by the target:

```c
#ifndef BASALT_ALIGNED_ALLOC_DECLARED
#define BASALT_ALIGNED_ALLOC_DECLARED 1
void *aligned_alloc(size_t alignment, size_t size);
#endif
```

Generated C MUST preserve source locations through `#line` directives where source mapping is enabled. Runtime support, allocation tracking, and cleanup code MAY make generated source larger than hand-written kernel code, but optimization MUST NOT change program output.

## Determinism and fixed point

The Bootstrap build pipeline is:

```text
src/bootstrap/basaltc.seed.c
        -> seed compiler binary
        -> translates src/bootstrap/basaltc.basalt
        -> current-generation C
        -> current compiler binary
```

A synchronized seed is valid only when the generated generations converge byte-for-byte. The repository’s production check compares the stable generations, normally `n3.c` and `n4.c`, and compares their SHA-256 with `src/bootstrap/fixed_point_production.sha256`. The seed, source emitter, serializer, mangling, runtime prologue, and diagnostics therefore form one deterministic contract.

## Conformance corpus

The Bootstrap compatibility corpus is stored under `tests/spec/`. Valid cases MUST compile, pass strict GCC, and produce the expected runtime result. Invalid cases MUST be rejected before a generated C artifact is created. The corpus is executed by `scripts/run_spec_compat.sh` and is also part of `scripts/run_ownership_stress.sh`.

## Compiler CLI and source mapping

The compatibility form of the Bootstrap compiler is `basaltc [--line|--no-line] <input.basalt> [output.c]`. Source mapping is enabled by default; `--line` explicitly enables it and `--no-line` disables it. When enabled, the C emitter MUST place `#line <source-line> "<escaped-source-file>"` at source-location transitions. The source file name MUST escape quotes, backslashes, and control characters. Disabling mapping MUST remove generated `#line` directives without changing program semantics or token emission.

The compiler also supports `basaltc --compile <input.basalt> [-o <binary>] [--cc <compiler>] [-- <compiler-arguments...>]`. In `--compile` mode, source mapping remains enabled by default so diagnostics and debugger locations continue to refer to the Basalt source. The caller MAY supply `--no-line` to suppress generated `#line` records, or `--line` to explicitly keep mapping enabled. The generated C path defaults to `<input.basalt>.c` in this mode, and the executable path defaults to `<input.basalt>.out`. The legacy second positional argument remains a C output path and MUST NOT be interpreted as an executable path. Compiler arguments after `--` are preserved as individual argv elements, are emitted before the generated input C path, and are followed by `-o` and the selected binary path. The implementation MUST NOT construct a shell command by concatenating quoted strings. A nonzero compiler exit status is returned to the caller and compiler stderr is forwarded for diagnosis.

## Structured process API

The standard library module `sys` exposes argv-oriented process execution without shell interpolation:

```basalt
include "../../src/stdlib/sys.basalt"

let args: array::Array<string> = array::new(0, "");
let result: sys::Output = sys::run("program", args, 65536);
```

`sys::Output` contains `status`, `succeeded`, `stdout`, `stderr`, `truncated`, and `spawn_error`. `args` contains only child arguments; each array element remains a distinct argv boundary, so whitespace and quotes are data rather than shell syntax. `status` is zero on success, a nonnegative normal exit code on ordinary failure, and negative on signal termination. A spawn or setup failure sets `spawn_error` to a positive platform error number and uses a negative status. `succeeded` is one only when status is zero and spawn_error is zero.

The `max_output` argument bounds stdout and stderr independently in bytes. Capturing more than the bound retains the prefix, sets `truncated` to one, and still drains both pipes and waits for the child to finish. Negative values and values above the implementation limit are rejected as invalid process requests. The API does not provide a shell mode; programs that intentionally need shell behavior MUST cross an explicit `includec`/FFI boundary and accept the platform and injection risks themselves. POSIX targets use fork/exec, pipes, polling, and wait semantics. Windows targets use `CreateProcessW`, convert each UTF-8 argument to UTF-16, construct a directly quoted Windows argv command line, and never invoke `cmd.exe`; stdout and stderr are captured through separate inherited pipes drained concurrently. A Windows spawn or setup failure is reported through the positive platform error value in `spawn_error`, while a normal child exit remains a status result. Both targets provide the same structured output shape and bounded-capture semantics.

## References

[1]: ../src/bootstrap/basaltc.basalt "Bootstrap Basalt compiler source"
[2]: ../src/bootstrap/basaltc.seed.c "Checked-in Bootstrap C seed"
[3]: DEVELOPER_GUIDE.md "Basalt Bootstrap developer workflow"
[4]: SAFE_IO_AND_DIAGNOSTICS.md "Basalt safety and diagnostics guidance"
