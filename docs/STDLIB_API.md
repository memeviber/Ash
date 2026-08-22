# Basalt Standard Library API Contract

This document defines the next standard-library surface for Basalt. The APIs are repository-side library modules backed by the Bootstrap compiler. They must not depend on the frozen OCaml compiler under `src/compiler/`.

## Design rules

Every owning container has an explicit `free` operation and returns its updated value after growth or mutation. Functions that may fail return `result::Result<T, int>` where `error` is a stable nonzero platform-independent category; `0` means success. Borrowed views are non-owning and must not outlive their source binding. C shims are limited to operating-system boundaries, preserve argument boundaries, and never interpret user strings as shell commands or format strings.

The first implementation targets strict C11 on POSIX and Windows UCRT/MinGW. Platform-specific code is isolated behind `#if defined(_WIN32)` in the generated C helper material. APIs that cannot have identical behavior on all platforms document their capability and error policy rather than silently emulating unsafe behavior.

## Filesystem

Module: `src/stdlib/filesystem.basalt`, namespace `filesystem`.

`open(path, mode)`, `read(file, max_bytes)`, `write(file, data)`, `close(file)`, and `metadata(path)` return `Result` values. A file handle is an opaque owned pointer and must be closed exactly once. `metadata` reports byte size, modification time in Unix seconds, and a kind tag (`1` regular file, `2` directory, `3` other). `directory(path)` returns an owning `array::Array<string>` containing entries without `.` and `..`. Directory and file names are returned as runtime-owned strings. `error_message(code)` provides a stable human-readable description for the common error categories. Directory exhaustion is reported internally as category `5`, and the wrapper consumes it rather than exposing it as a failure.

The module never follows a caller-controlled shell command. `read` is a text-oriented API: it reads at most `max_bytes`, appends a NUL terminator, and therefore is not a binary-buffer interface for embedded NUL bytes. It rejects invalid sizes and treats short writes as errors rather than claiming completion.

## Path

Module: `src/stdlib/path.basalt`, namespace `path`.

`separator()`, `join(left, right)`, `normalize(value)`, `extension(value)`, and `basename(value)` operate on byte strings and accept both `/` and `\\` as input separators. Output uses the current platform separator. `extension` and `basename` return empty strings when no component exists. `normalize` removes `.` components, resolves `..` lexically without accessing the filesystem, preserves an absolute root, and does not follow symlinks.

## String

Module: `src/stdlib/string.basalt`, namespace `str`.

Existing byte-oriented and UTF-8 validation APIs remain stable. The new `find`, `starts_with`, `ends_with`, `split`, `replace`, and `trim` APIs are byte-oriented unless their name explicitly contains `utf8`. `find` returns a byte offset or `-1`; `split` returns an owning `array::Array<string>`; `replace` performs non-overlapping left-to-right replacement; `trim` removes ASCII whitespace.

`StringView` is a non-owning `{source, offset, len}` byte range. It is not NUL-terminated, cannot be passed as a `string`, and must be used only while the source string remains alive. `view`, `view_len`, `view_byte_at`, and `view_to_string` provide the checked boundary; `view_to_string` returns a new owned string.

## Collections

`array::sort_stable` and `slice::sort_stable` use a stable comparator-driven insertion sort. The comparator must return true only when its first argument belongs before its second argument. This initial implementation favors deterministic behavior and small code over asymptotic performance.

Module `src/stdlib/set.basalt` provides generic `Set<K>` over the existing typed hash map: `new`, `with_hasher`, `insert`, `contains`, `remove`, `clear`, `len`, `is_empty`, and `free`.

Module `src/stdlib/deque.basalt` provides an owning generic ring buffer: `new`, `push_front`, `push_back`, `pop_front`, `pop_back`, `front_or`, `back_or`, `len`, `capacity`, `is_empty`, `clear`, and `free`. `push_*`, `clear`, and `free` return the updated deque. Each `pop_*` returns `PopResult<T>`, containing the updated deque and an `option::Option<T>` payload, so mutation is never lost through by-value struct passing.

`iter.basalt` adds explicit array, slice, map, set, and deque iterator structs. Each `next` returns a `NextResult` containing the advanced cursor and an `Option` payload because Basalt structs are passed by value. Iterators are lightweight borrowed cursors and do not own the underlying container; mutation or freeing the source invalidates the cursor.

## Time

Module `src/stdlib/time.basalt`, namespace `time`.

`monotonic_ns()` returns a nondecreasing monotonic clock when the platform provides one. `wall_seconds()` returns Unix wall-clock seconds. `duration_ms(milliseconds)` constructs a duration, `elapsed(start, duration)` checks a deadline using the monotonic clock, and `sleep_ms(milliseconds)` yields for a bounded duration. Invalid negative durations return an error where applicable.

## Process and environment

Module `src/stdlib/process.basalt`, namespace `process`.

`getenv`, `setenv`, `unsetenv`, `cwd`, and `chdir` use `Result` values and copy returned strings into owned runtime storage. `stdin_line(max_len)` delegates to bounded `io::read_line`. `spawn(executable, args)`, `wait`, `wait_timeout`, `send_signal`, and `free` operate on an opaque process handle without a shell. POSIX spawn reports `exec` failure through an internal close-on-exec pipe instead of presenting exit `127` as a successful spawn. A handle must be waited before `free`; freeing an un-waited handle returns lifecycle error `6` so it cannot silently become a zombie. Timeout and signal return explicit unsupported error `8` on Windows where the implementation cannot provide the same contract.

## Concurrency

`concurrency.basalt` retains the current atomics, bounded SPSC integer channel, and explicit thread join API. It adds an opaque mutex (`mutex_make`, `mutex_lock`, `mutex_unlock`, `mutex_free`) and cancellation token (`cancel_make`, `cancel_request`, `cancelled`, `cancel_free`). Mutex operations are blocking and return status codes; cancellation is cooperative and never forcibly terminates a thread.

## Formatting

Module `src/stdlib/format.basalt` provides an owning builder with typed append operations for strings, characters, integers, 64-bit integers, and floating-point values. It does not expose C varargs or accept user input as a C format string. `finish` returns `FinishResult { formatter, value }`; the returned formatter contains a reset builder and the value is an owned string. Callers must assign the returned formatter before freeing it.

## Randomness

Module `src/stdlib/random.basalt`, namespace `random`.

`Rng` is a deterministic SplitMix64 generator with `seed`, `try_seed`, `next_u64`, `next_u32`, `next_bounded`, and `next_float`. `try_seed` exposes allocation failure as `Result<Rng, int>` while `seed` remains the compatibility convenience constructor. `entropy_u64` reads the operating-system entropy source (`/dev/urandom` on POSIX and the UCRT secure random facility on Windows) and returns `Result<u64, int>`. Failure is reported; time or pointer values are not silently presented as cryptographic entropy.

## Compatibility and testing

Existing module names and APIs remain source-compatible except for the ownership-safe return shape of `string_builder::finish` and `format::finish`: callers must now assign the returned `FinishResult.formatter` and consume its owned `value`. New modules are opt-in through `include` until the standard-library packaging policy gains a canonical module resolver. Each public group must have normal, edge, invalid, ownership, and platform-compile fixtures. All generated C and binaries belong under `.tmp`; the regression harness must run the package and standard-library tests without network access and without touching `src/compiler/`.
