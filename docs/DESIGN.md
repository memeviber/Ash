# Ash Design Notes

## Architecture

Ash has a reference Host implementation and a self-hosting Bootstrap implementation. The Host compiler is written in OCaml because it provides a concise platform for validating grammar, type rules, and C generation. The Bootstrap compiler is written in Ash and exercises the language's own lexer, parser, AST arena, type checker, symbol table, generic specialization, and C serializer.

The two implementations are intentionally kept structurally close. A language change is complete only when the Host and Bootstrap paths accept the same valid programs, reject the same invalid programs, generate compilable C11, and produce equivalent runtime behavior.

## Pipeline

The pipeline is divided into the following stages:

| Stage | Responsibility |
| --- | --- |
| Lexer | Converts source bytes into tokens, including `MOD` for `%` |
| Parser | Builds expression and declaration nodes using precedence tables |
| AST | Stores nodes and type payloads in deterministic arenas |
| Type checker | Resolves names, scopes, generics, fields, ownership, and operator constraints |
| Specializer | Monomorphizes generic functions and types used by a program |
| C generator | Emits an intermediate C-token stream and serializes it to C11 |
| Validation | Compiles with strict diagnostics and executes under sanitizers |

## Determinism

The emitter uses stable symbol IDs and deterministic traversal order. Generic specialization collection uses a warm-up generation pass, followed by two generation passes whose token counts are compared before the final C source is accepted. The warm-up is necessary because nested namespace-qualified generic calls can discover additional dependent specializations during the first traversal. The fixed-point script then compiles the Bootstrap compiler through successive generations and compares the complete `n2.c` and `n3.c` files byte-for-byte.

## Modulo implementation

The modulo feature crosses the complete compiler boundary. The Host AST defines `Mod`, the Host lexer recognizes `%`, and the parser assigns it multiplicative precedence. The Host type checker applies the arithmetic numeric rule and the C emitter writes `%`.

The Bootstrap implementation mirrors this path with `OP_MOD = 17`, `T_MOD = 62`, and `L_MOD = 64`. The lexer maps byte value 37 to `L_MOD`; `map_token` maps it to `T_MOD`; the precedence and operator tables map it to `OP_MOD`; the type checker handles it with the multiplication and division branch; and `emit_c_token` writes the percent character for operator value 17.

## Memory model

The generated runtime tracks heap allocations in a registry and releases outstanding allocations at process exit. Explicit container and resource release operations remove entries from the registry. Resizing functions update tracked pointers after `realloc`, and sanitizer-oriented tests exercise duplicate release, untracked release, cycles, nested includes, dynamic arrays, and string concatenation.

The current model is intentionally pragmatic. It offers runtime safety checks while leaving room for a future ownership and borrowing system with more precise lifetime semantics.

## Portability

The C emitter avoids relying on platform-specific runtime behavior where practical. Include path handling uses canonical paths on POSIX and the corresponding Windows path routine. Release builds require strict GCC diagnostics, while the generated C remains ordinary C11 suitable for other conforming compilers.
