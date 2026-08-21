# Basalt Language Specification

## Status and conformance

This document is the normative language contract for the Bootstrap Basalt compiler. The implementation of record is `src/bootstrap/basaltc.bsl` together with the checked-in C seed `src/bootstrap/basaltc.seed.c`; the frozen seed must be able to compile the Bootstrap source without invoking the Host compiler. A language change is conforming only when the Bootstrap compiler accepts every valid corpus case, rejects every invalid corpus case before C emission, produces strict-C11 output, and preserves the fixed-point property described below.

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

A block introduces a scope. Local bindings declared in an inner scope MUST NOT remain visible after the scope closes. The type checker tracks ownership state separately from lexical name lookup. A moved owner MUST NOT be used again. A borrowed value MUST NOT outlive its owner, and a borrowed owner MUST NOT be mutated while an incompatible active borrow exists. The current Bootstrap implementation provides scope-based borrow checking; explicit lifetime parameters and closure-capture lifetime inference are reserved for a later language revision and MUST NOT be assumed by current programs.

`defer statement;` registers cleanup in the active scope. Deferred operations MUST execute in reverse registration order on the corresponding exit path, including explicit returns covered by the implementation. A resource transferred out of a scope MUST NOT also be freed by the originating scope.

The `null` literal represents a null pointer value. A raw zero MAY be accepted in pointer contexts covered by the C-oriented compatibility rules, but `null` is the portable spelling for new code.

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

`include "module.bsl";` loads Basalt source. Include resolution MUST use the including file as the base for relative paths, canonicalize paths before cycle checks, reject active include cycles, and avoid processing the same canonical module more than once.

`includec "file.h";` injects or includes raw C material through the emitter. It is an escape hatch and MUST be isolated from ordinary Basalt type checking. New FFI declarations SHOULD use `extern` so the function signature and ownership boundary remain visible to the compiler.

An `extern` declaration declares a C-provided function with a Basalt signature. The current implementation checks the Basalt-side arity and type contract; C-side ABI verification remains a portability responsibility of the build configuration until the controlled FFI milestone adds header and symbol validation.

## Diagnostics contract

Every compile-time failure MUST produce a nonzero exit status and MUST NOT leave a C artifact at the requested output path. The Bootstrap formatter emits a concise message followed by stable fields: `diagnostic.code`, `diagnostic.file`, one-based `diagnostic.line`, one-based `diagnostic.column`, `diagnostic.excerpt`, and `diagnostic.hint`. The file and excerpt are derived from the source byte span associated with the first failure; included files MUST report their own canonicalized source path.

For type mismatch codes 12, 20, 21, 23, and 36, the formatter MUST additionally emit `diagnostic.expected` and `diagnostic.found` with human-readable type names. The compiler MUST preserve the first failing span while type checking so later traversal cannot overwrite the originating location. Diagnostic wording MAY evolve, but stable codes, field labels, rejection behavior, and source-location semantics are compatibility requirements. Tests MUST assert semantic rejection and the documented structured fields rather than depending only on incidental prose.

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
        -> translates src/bootstrap/basaltc.bsl
        -> current-generation C
        -> current compiler binary
```

A synchronized seed is valid only when the generated generations converge byte-for-byte. The repository’s production check compares the stable generations, normally `n3.c` and `n4.c`, and compares their SHA-256 with `src/bootstrap/fixed_point_production.sha256`. The seed, source emitter, serializer, mangling, runtime prologue, and diagnostics therefore form one deterministic contract.

## Conformance corpus

The Bootstrap compatibility corpus is stored under `tests/spec/`. Valid cases MUST compile, pass strict GCC, and produce the expected runtime result. Invalid cases MUST be rejected before a generated C artifact is created. The corpus is executed by `scripts/run_spec_compat.sh` and is also part of `scripts/run_ownership_stress.sh`.

## References

[1]: ../src/bootstrap/basaltc.bsl "Bootstrap Basalt compiler source"
[2]: ../src/bootstrap/basaltc.seed.c "Checked-in Bootstrap C seed"
[3]: DEVELOPER_GUIDE.md "Basalt Bootstrap developer workflow"
[4]: SAFE_IO_AND_DIAGNOSTICS.md "Basalt safety and diagnostics guidance"
