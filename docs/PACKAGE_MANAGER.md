# Basalt Package Manager

## Status

This document defines the package-manager contract for Basalt. The implementation is deliberately a repository tool around the Bootstrap compiler; it does not modify or invoke the frozen OCaml Host compiler. Package resolution, reproducibility, and artifact verification are separated from compiler import syntax, while the Bootstrap compiler provides explicit `@lib` and `@stdlib` aliases for the verified source layout.

The package manager uses five persistent concepts:

| Concept | File or location | Purpose |
|---|---|---|
| Manifest | `Basalt.toml` | Human-maintained package metadata and dependency requirements |
| Lockfile | `Basalt.lock` | Machine-maintained exact dependency graph and checksums |
| Registry | Registry base URL | Discovery metadata and immutable package archives |
| Cache | `$BASALT_HOME/cache` | Content-addressed downloaded archives and extracted packages |
| Vendor tree | `.basalt/vendor` | Verified source material used by a build |

## Manifest

A package root contains `Basalt.toml`. The minimum package is:

```toml
[package]
name = "example"
version = "0.1.0"
edition = "2026"
entry = "src/main.basalt"

[dependencies]
text = "^1.2.0"

[dev-dependencies]
assertions = "^0.3.0"
```

The package name must contain only lowercase ASCII letters, digits, and hyphens, begin with a letter, and be at most 64 characters. The version is Semantic Versioning 2.0.0. The entry path must remain inside the package root and have the `.basalt` extension.

A dependency may use the expanded form when a non-default registry or a local path is required:

```toml
[dependencies]
text = { version = ">=1.2.0, <2.0.0", registry = "https://registry.example.invalid" }
local_math = { path = "../local_math" }
```

The first implementation accepts `version`, `registry`, and `path`. Git dependencies, arbitrary URLs, build scripts, native library hooks, and executable package hooks are intentionally not part of the initial contract.

## Version requirements

The resolver supports the following deterministic SemVer requirement forms:

| Form | Meaning |
|---|---|
| `1.2.3` | Exact version |
| `^1.2.3` | Compatible release with the same major version |
| `~1.2.3` | Compatible release with the same major and minor version |
| `>=1.2.3` | Lower inclusive bound |
| `>1.2.3` | Lower exclusive bound |
| `<=1.2.3` | Upper inclusive bound |
| `<1.2.3` | Upper exclusive bound |
| `1.2` | Equivalent to `>=1.2.0, <1.3.0` |
| `*` | Any stable version |
| comma-separated terms | Logical AND |

Pre-release versions are not selected by a requirement that does not itself contain a pre-release identifier. When multiple versions satisfy a requirement, the resolver selects the highest version and then resolves dependencies in lexical package-name order. A lockfile always takes precedence over the manifest until `update` is requested.

## Registry protocol

A registry is an HTTPS base URL with two read-only resources for each package:

```text
GET <base>/index/<package-name>.json
GET <base>/packages/<package-name>-<version>.tar.gz
```

The index response is a JSON array of immutable release records:

```json
[
  {
    "name": "text",
    "version": "1.2.3",
    "archive": "packages/text-1.2.3.tar.gz",
    "checksum": "sha256:...",
    "dependencies": {}
  }
]
```

The archive must contain one top-level directory named `<name>-<version>/` and a `Basalt.toml`. Registry metadata is advisory until the downloaded archive hash matches the declared SHA-256 value and the archive manifest matches the index record. The package manager never executes code while fetching, extracting, or verifying a package.

A registry implementation may later add signed index metadata. The first contract treats the HTTPS transport and SHA-256 archive checksum as integrity checks, not as a complete publisher identity system.

## Lockfile

`Basalt.lock` is generated and must not be edited manually. The initial format is TOML:

```toml
format = 1
root = "example 0.1.0"

[[package]]
name = "text"
version = "1.2.3"
source = "registry+https://registry.example.invalid"
checksum = "sha256:..."
dependencies = []
```

Each package record contains `name`, `version`, `source`, `checksum`, and a sorted `dependencies` array. Path dependencies use a normalized repository-relative source such as `path+../local_math`; they do not receive a registry checksum unless they are later archived. The lockfile is valid only when:

1. every package name and version is valid SemVer;
2. every dependency edge points to a package record;
3. package records are sorted by name and version;
4. every registry package has a SHA-256 checksum;
5. the root package matches `Basalt.toml`.

Build and test commands use the lockfile exactly. `update` is the explicit operation that may choose newer satisfying versions and rewrite checksums.

## Cache and vendor materialization

The default cache is `$BASALT_HOME/cache`, where `$BASALT_HOME` defaults to `$HOME/.basalt`. Tests and CI must set `BASALT_HOME` to a directory under `.tmp`. Archives are stored by checksum:

```text
$BASALT_HOME/cache/sha256/<first-two-hex>/<full-hex>.tar.gz
```

A cache hit is accepted only after hashing the cached archive. A corrupt or mismatched object is deleted and downloaded again. Verified archives are extracted into a temporary directory, checked for path traversal, checked for the required top-level directory and manifest, and then atomically moved into:

```text
.basalt/vendor/<name>/<version>/
```

The vendor directory is disposable build material. It is not part of the lockfile and should normally be ignored by Git. `fetch --offline`, `verify`, and `build --offline` use only the lockfile and checksum-addressed cache; they fail closed if a locked archive is missing or mismatched and never fall back to a registry.

## Commands

The initial command surface is:

| Command | Purpose |
|---|---|
| `basalt-pkg init` | Create a `Basalt.toml` skeleton |
| `basalt-pkg add NAME@REQ` | Add or update a manifest dependency |
| `basalt-pkg fetch` | Resolve the lockfile, verify archives, and materialize vendor sources |
| `basalt-pkg update [NAME...]` | Re-resolve selected dependencies and rewrite `Basalt.lock` |
| `basalt-pkg build` | Fetch verified sources and invoke Bootstrap `basaltc --compile` |
| `basalt-pkg tree` | Print the locked dependency graph |
| `basalt-pkg verify` | Verify manifest, lockfile, cache, checksums, and source paths without compiling |
|

The current implementation exposes this command as `scripts/basalt_pkg.py` so package resolution can be tested without changing compiler semantics. A future installed `basalt` launcher may delegate to the same implementation. Global options precede the subcommand, for example `python3 scripts/basalt_pkg.py --root . --registry ./registry fetch`; `--offline` is a subcommand option for `fetch` and `build`.

## Build integration boundary

The package manager materializes verified dependencies at `.basalt/vendor/<name>/<version>/`. The Bootstrap compiler exposes that exact layout through the explicit prefix `@lib/<name>/<version>/<entry>`. The standard library is exposed through `@stdlib/<entry>`, which resolves to `<project-root>/src/stdlib/<entry>`. The compiler captures `<project-root>` from its working directory when translation starts, so `build` invokes it from the package root and nested includes keep a stable root.

Prefix expansion happens before canonical-path registration and therefore preserves cycle detection, duplicate-load suppression, dependency edges, and diagnostic target paths. The version component is mandatory and is not inferred from the lockfile. A legacy quoted relative include remains including-file-relative and remains supported; prefix syntax does not rewrite or invalidate it. `build` passes the Bootstrap compiler path, `--compile`, entry path, `--cc` C compiler, output path, and each repeated `--compiler-arg` as separate argv elements; it never concatenates a shell command. Use `--compiler` or `BASALT_COMPILER` for the Bootstrap compiler and `--cc` or `CC` for the generated-C compiler.

## Security contract

The package manager must:

- keep registry and archive paths separate from shell command strings;
- pass compiler arguments as an argv vector;
- reject archive path traversal, absolute archive paths, and symlink escapes;
- verify SHA-256 before extraction and again before build;
- reject duplicate package identities with conflicting checksums;
- never execute package-provided scripts during fetch or build;
- avoid following dependency metadata outside the selected registry or declared path;
- keep temporary downloads and generated binaries under `.tmp` during repository tests.

A package is source input, not trusted executable input. `includec` remains an explicit unsafe C boundary and is not made implicit by package installation.

## Bootstrap-only release procedure

Package-manager changes must follow the existing rule:

```sh
bash scripts/bootstrap_stage.sh
bash scripts/fixed_point.sh
bash scripts/run_regression.sh
bash scripts/run_ownership_stress.sh
```

The package-manager tool itself is validated separately with a temporary `BASALT_HOME` under `.tmp`. No release step may build, modify, or invoke `src/compiler/`.

## Deferred features

The following are intentionally deferred until the initial resolver and lockfile are stable:

- signed registry metadata and publisher keys;
- git and arbitrary URL dependencies;
- native C library packages and build scripts;
- workspace packages and multiple binary targets;
- incremental compilation and binary artifact caching;
- platform-specific dependency conditions.
