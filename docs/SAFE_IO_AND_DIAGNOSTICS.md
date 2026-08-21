# Safe I/O and Bootstrap Diagnostics

## Safe input/output API

The Bootstrap standard library now exposes `src/stdlib/io.basalt` under the `io` namespace. The implementation is backed by bounded C runtime primitives emitted by the Bootstrap compiler; it does not use unbounded `scanf`, `%s`, or an unchecked line reader.

| API | Contract |
|---|---|
| `io::read_line(max_len): string` | Requires `2 <= max_len <= 1048576`; reads at most `max_len - 1` bytes, appends a NUL terminator, and returns an owned string. A longer physical line is discarded through its newline and reports status `3`. |
| `io::read_int(fallback): int` | Reads one physical line into a fixed 128-byte buffer, parses a complete base-10 integer, and returns `fallback` for malformed, oversized, out-of-range, or EOF input. |
| `io::status(): int` | Returns the status of the most recent input operation. |
| `io::write(value): void` | Writes a non-null string and flushes stdout; a write failure terminates with a controlled runtime failure. |
| `io::writeln(value): void` | Writes a string, a newline, and flushes stdout. |
| `io::write_int(value): void` | Writes an `int` and flushes stdout. |
| `io::write_char(value): void` | Writes one character and flushes stdout. |

The status values are deliberately small and stable:

| Status | Meaning |
|---:|---|
| `0` | Operation succeeded. |
| `1` | EOF was reached before any input was received. |
| `2` | Integer input was malformed or had non-whitespace trailing characters. |
| `3` | The input exceeded the configured bound and the remainder of the line was discarded. |
| `4` | The integer was outside the representable `int` range. |

“100% safe input” is implemented here as a bounded and checked input boundary: the input cannot overwrite a caller-owned buffer, `read_int` cannot grow beyond its fixed local buffer, line allocation is checked, invalid limits are rejected, and allocation is registered with the existing runtime ownership tracker. No external input API can honestly guarantee that the operating system, filesystem, or stdout will never fail, so those failures are converted into controlled runtime termination rather than undefined behavior.

## Why the compiler prints numbers

The numbers are not one single encoding. They come from several independent diagnostic channels.

### 1. `diagnostic.code`

When compilation fails during type checking, the compiler first prints a human-readable message and then prints the numeric internal error code. The number is an enum-like stable identifier used by the Bootstrap compiler and its tests.

| Code | Current meaning |
|---:|---|
| `0` | Parse error or no typechecker code was available. |
| `3` | Duplicate declaration. |
| `5` | Unknown name. |
| `12` | Invalid function arguments. |
| `13` | Invalid argument count. |
| `14` | String concatenation requires strings. |
| `17` | Invalid built-in argument type. |
| `18` | Invalid arithmetic operands. |
| `20` | Initializer type mismatch. |
| `21` | Assignment type mismatch. |
| `23` | Return type mismatch. |
| `28` | Recursive struct definition. |
| `31` | Assignment to `const`. |
| `33`/`34` | Use after ownership move. These two legacy paths currently share the same diagnostic text. |
| `35` | Release requires an owned value. |
| `36` | Array element type mismatch. |
| `37` | Cannot mutate or move while borrowed. |
| `38` | Borrowed reference escapes its owner. |
| `40` | Owned value copy requires an explicit move. |
| `41` | Unknown function. |
| `42` | A non-function value was called. |
| `43` | Function name is reserved by the C runtime. |
| `44` | Distinct functions collide after C name mangling. |
| `45` | Array index is out of bounds. |

Some older internal failure paths fall through to the generic `type error: invalid expression` text. They are still rejected; the table above documents the codes that currently have dedicated diagnostic messages.

### 2. `diagnostic.line` and `diagnostic.column`

These are **one-based source coordinates**, not error codes. `line` counts newline characters and `column` counts characters from the beginning of the current line. Because `include` expansion happens before parsing, the current implementation reports coordinates in the expanded compiler input. This is why a diagnostic in an included file can have a line number larger than the visible line number in the original root file.

### 3. Token-debug numbers

The Bootstrap source contains a disabled `debug_tokens` switch. It is `0` by default, so a normal successful compilation does not dump token numbers. When explicitly enabled in a debug build, each token is printed with labels:

| Field | Meaning |
|---|---|
| `token.kind` | Internal `T_*` token ID, such as identifier, integer, `)` or semicolon. |
| `token.start` | Byte offset into the expanded source buffer. |
| `token.length` | Lexical width of the token in bytes. |
| `token.value` | Payload, for example an integer value, symbol ID, or character value. |

Those values are compiler metadata, not generated program output and not memory addresses.

### 4. Generator regression result

The internal code-generator regression helper previously printed a bare `1` for success and `0` for failure. It now prints `generator_regression: PASS` or `generator_regression: FAIL`, so this channel is no longer ambiguous.

## Reading a typical failure

```text
type error: invalid argument count
diagnostic.code
13
diagnostic.line
24
diagnostic.column
17
```

This means the compiler rejected the program because a function was called with the wrong number of arguments. `13` is the diagnostic category; `24` and `17` identify the one-based location in the expanded source. None of these numbers is a runtime result from the program being compiled.

All modern checks described here use the current-generation Bootstrap compiler produced by the stored Bootstrap C seed. The frozen Host OCaml compiler is not part of the build or test path.
