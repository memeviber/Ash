# Basalt Language Specification

## 1. Scope

Basalt is a statically typed, C-emitting language designed for small systems programs, compiler implementation, and data-oriented utilities. Its reference behavior is defined by the Host compiler and checked against the Bootstrap compiler. The language deliberately keeps the core syntax compact while exposing pointers, C interoperability, generics, and explicit container operations.

## 2. Program structure

A program is a sequence of type declarations, namespace declarations, external declarations, includes, global declarations, and function definitions. The conventional entry point is:

```basalt
func main(): int {
  return 0;
}
```

Statements use semicolons. Blocks are delimited by braces. Conditional statements use `if condition then { ... } else { ... }`; loops use `while condition { ... }`. `for` is supported by the compiler and is lowered to equivalent C control flow. `break` and `continue` apply to the innermost loop.

## 3. Lexical forms

Identifiers contain letters, digits, and underscores, subject to the usual rule that the first character is not a digit. Integer literals are decimal. String literals use double quotes and character literals use single quotes. Escape sequences include the common control escapes and escaped quote and backslash forms. Comments are line comments beginning with `//`.

## 4. Types

The primitive types are `int`, `bool`, `char`, `string`, `float`, `double`, and `void`. Pointers use postfix `*`, fixed arrays use a type and compile-time length, and dynamic arrays use the generic array facilities in the standard library. Named types include structs and enums. Generic types use angle brackets, for example `Result<int, string>`.

Basalt treats `int` and `bool` as compatible in the current C-oriented type model. Character values are integer-like. `float` and `double` participate in numeric compatibility and array element matching according to the compiler's strict element rules. Pointer compatibility follows C-like rules for compatible pointee types and `void*` conversions.

## 5. Expressions and precedence

The precedence levels below are listed from lowest to highest. Operators on the same row associate left-to-right unless stated otherwise.

| Level | Operators | Meaning |
| --- | --- | --- |
| 1 | `||` | logical OR |
| 2 | `&&` | logical AND |
| 3 | `==`, `!=`, `<`, `>` | comparison |
| 4 | `++` | string concatenation |
| 5 | `\|` | bitwise OR |
| 6 | `^` | bitwise XOR |
| 7 | `&` | bitwise AND |
| 8 | `<<`, `>>` | shifts |
| 9 | `+`, `-` | addition and subtraction |
| 10 | `*`, `/`, `%` | multiplication, division, and modulo |

The modulo operator returns the C11 remainder of its integer operands. It has the same precedence as multiplication and division. The Host and Bootstrap parsers use the same token, precedence, AST opcode, type-checking branch, and C emission mapping.

> Example: `a + b % c` parses as `a + (b % c)`, while `(a + b) % c` explicitly applies modulo to the sum.

Unary `-` is available for numeric expressions. `&value` takes an address and `*pointer` dereferences a pointer. Array indexing uses `array[index]`, and struct field access uses `value.field`.

## 6. Declarations and assignment

A mutable local declaration has the form `let name: Type = expression;`. `const` creates a read-only binding. Assignment uses `name = expression;`, and field or indexed assignment follows the corresponding access expression. The type checker rejects incompatible initializers, assignments, returns, calls, field operations, and array element operations before C emission.

The `null` literal represents a null pointer value and is emitted through the C runtime's portable null representation. A raw integer zero remains accepted in the C-oriented pointer compatibility rules where specified by the implementation.

## 7. Functions, namespaces, and generics

Functions declare parameters and a return type:

```basalt
func remainder(a: int, b: int): int {
  return a % b;
}
```

Namespaces introduce a separate qualified scope. A declaration in `namespace result { ... }` is called as `result::ok(...)`. Generic structs and functions use type parameters, which are instantiated and monomorphized for C emission. The generated C name uses `__` as the namespace and specialization separator.

## 8. Structs and enums

Structs contain typed fields:

```basalt
struct Point {
  x: int;
  y: int;
}
```

Enums contain named variants. The type checker validates field existence and field assignment types. Recursive structs are permitted through pointers but not by value, preventing infinite object layouts.

## 9. Arrays and standard library

The standard library provides generic dynamic-array operations and container modules. Implementations track allocation ownership in the generated runtime and provide explicit release operations for resources that escape ordinary local cleanup. `map` and `result` are generic namespace-based modules; their public names do not carry an artificial language prefix.

## 10. C interoperability and includes

`extern` declares a C-provided function with an Basalt signature. `includec "file.h";` injects or includes raw C material under the compiler's controlled emission path. `include "module.bsl";` loads another Basalt source file. Recursive include processing tracks canonical paths, rejects active include cycles, and avoids duplicate loaded modules.

## 11. Diagnostics

The Host compiler reports human-readable type and parse errors. The Bootstrap compiler records an error code and source position, reports a corresponding diagnostic, and exits nonzero without writing a C artifact. The regression suite accepts differences in wording while requiring equivalent rejection behavior.

## 12. Generated C contract

Basalt emits portable C11. Release validation uses:

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror
```

The Bootstrap fixed-point criterion is byte identity between the C generated by generations two and three. This criterion protects deterministic parsing, specialization, symbol mangling, runtime serialization, and operator emission.
