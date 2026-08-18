# Naming Policy

Basalt repository names should describe the language concept directly and should not add a redundant language prefix to standard-library APIs. Generic modules therefore use names such as `map`, `result`, and `string`, while namespace qualification provides context such as `map::new` or `result::ok`.

Source files use lowercase snake case for compiler internals and standard-library modules. Public type names use PascalCase, for example `HashMap<K, V>` and `Result<T, E>`. Functions and variables use lowercase snake case. Constants use uppercase snake case when they represent compile-time or protocol values.

The repository-facing product name is **Basalt**. Historical implementation filenames may remain when they are part of the bootstrap chain, but comments, documentation, scripts, and user-facing diagnostics must describe the compiler and language as Basalt.

Generated C uses stable mangling: namespace separators and generic specialization separators become `__`, while type names use explicit, deterministic components. Mangling is an implementation detail and is not part of the Basalt source-level API.
