# Windows UCRT64 Bootstrap Portability

This guide explains how to compile the Bootstrap compiler's generated C on Windows from an **MSYS2 UCRT64** shell. The Bootstrap emitter now selects a pthread compatibility shim automatically for MinGW/UCRT64 targets when the generated C would otherwise depend on an unavailable `<threads.h>` implementation.

The repository's source of truth remains `src/bootstrap/basaltc.basalt`. The emitter emits the platform conditionals, pthread adapter, and guarded `aligned_alloc` declaration into every generated C translation. Users should not patch `basaltc.c` or `basaltc.seed.c` by hand; after emitter changes, regenerate the seed and verify the fixed point.

## Toolchain setup

Open the **MSYS2 UCRT64** terminal, not the MSYS or MINGW32 terminal. Install the compiler and the UCRT64 winpthreads package:

```sh
pacman -Syu
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain \
  mingw-w64-ucrt-x86_64-winpthreads
```

The MSYS2 package contains `pthread.h`, `sched.h`, and the UCRT64 pthread libraries, including `libpthread.a` and `libwinpthread.a` [1]. Confirm that the selected shell resolves the expected compiler:

```sh
which gcc
gcc --version
gcc -dumpmachine
```

The final command should identify a UCRT64 MinGW target rather than an MSYS or unrelated native Windows compiler.

## Why `<threads.h>` can fail

Some UCRT64 headers do not expose a usable `<threads.h>` even though the installed winpthreads package supplies a POSIX-compatible thread layer. The Bootstrap emitter therefore emits `pthread.h` and `sched.h`, defines the C11-shaped `thrd_*` surface locally, and delegates to `pthread_create`, `pthread_join`, and `sched_yield`. On Linux and macOS, the generated runtime keeps the native `<threads.h>` path. The Windows branch is selected by `_WIN32 && __MINGW32__`; defining `BASALT_USE_NATIVE_C11_THREADS` opts out of the adapter when a target provides a working native C11 threads implementation.

GCC documents `-pthread` as the thread-support option and explicitly lists MinGW targets among the supported environments [2]. Prefer `-pthread` for both compilation and linking. An explicit `-lpthread` link option is also shown below for toolchains that require it, but it does not always provide the same preprocessor configuration as `-pthread`.

## Safe pthread compatibility shim

The following block is now emitted automatically in the generated C runtime prologue **before** the Basalt runtime uses `thrd_t` or any `thrd_*` function. It is shown here to document the generated interface, not as a manual patch step. The adapter is safer than frequently copied one-line macros because `pthread_create` expects a `void *(*)(void *)` entry point, whereas a C11 thread entry point returns `int`. It also avoids casting an `int *` to `void **` for `pthread_join`, which can overwrite more bytes than the destination integer on 64-bit Windows.

```c
#if defined(_WIN32) && defined(__MINGW32__) && !defined(BASALT_USE_NATIVE_C11_THREADS)
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>

typedef pthread_t thrd_t;
typedef int (*basalt_thrd_start_t)(void *);
#define thrd_success 0

typedef struct basalt_thrd_start_context {
    basalt_thrd_start_t entry;
    void *argument;
    int result;
} basalt_thrd_start_context;

static void *basalt_thrd_start(void *raw) {
    basalt_thrd_start_context *context = (basalt_thrd_start_context *)raw;
    context->result = context->entry(context->argument);
    return raw;
}

static int basalt_thrd_create(thrd_t *thread, basalt_thrd_start_t entry, void *argument) {
    basalt_thrd_start_context *context;
    int status;

    context = (basalt_thrd_start_context *)malloc(sizeof(*context));
    if (context == NULL) {
        return 1;
    }
    context->entry = entry;
    context->argument = argument;
    context->result = 0;
    status = pthread_create(thread, NULL, basalt_thrd_start, context);
    if (status != 0) {
        free(context);
    }
    return status;
}

static int basalt_thrd_join(thrd_t thread, int *result) {
    void *raw = NULL;
    int status = pthread_join(thread, &raw);
    if (status == 0) {
        basalt_thrd_start_context *context = (basalt_thrd_start_context *)raw;
        if (result != NULL) {
            *result = context->result;
        }
        free(context);
    }
    return status;
}

#define thrd_create(thread, entry, argument) \
    basalt_thrd_create((thread), (entry), (argument))
#define thrd_join(thread, result) \
    basalt_thrd_join((thread), (result))
#define thrd_yield() sched_yield()
#else
#include <threads.h>
#endif
```

This adapter allocates one small context record per started thread and releases it after a successful join. The thread entry result is copied into the caller's `int` rather than being transported through a `void **` cast.

## `aligned_alloc` declaration

Some Windows compiler/header combinations do not expose the C11 prototype even when the selected MinGW runtime provides a compatible symbol. The C11 declaration is:

```c
#include <stddef.h>

void *aligned_alloc(size_t alignment, size_t size);
```

`aligned_alloc` is specified in `<stdlib.h>` and requires `size` to be an integral multiple of `alignment` [3]. Basalt's aligned allocation path already rounds the requested byte count before calling it. If the header is missing the prototype but the runtime exports the function, the declaration can be placed in the portability section after `<stddef.h>` and before the first call:

```c
#ifndef BASALT_ALIGNED_ALLOC_DECLARED
#define BASALT_ALIGNED_ALLOC_DECLARED 1
#include <stddef.h>
void *aligned_alloc(size_t alignment, size_t size);
#endif
```

A declaration alone does **not** provide an implementation. If the link step reports an unresolved `aligned_alloc`, use the implementation supplied by the selected UCRT64 toolchain or add a platform-specific allocator abstraction whose deallocator matches the allocator. Do not replace `aligned_alloc` with `_aligned_malloc` while continuing to release the pointer with ordinary `free`; those APIs have matching allocation/deallocation families and must not be mixed.

## Bootstrap compile commands

For a temporary generated `basaltc.c`, use the strict repository warning profile. The preferred command is:

```sh
gcc -std=c11 -O3 -Wall -Wextra -Wpedantic \
  -Wconversion -Wshadow -Werror \
  -pthread basaltc.c -o basaltc.exe
```

If the UCRT64 toolchain does not accept `-pthread` for the particular installation, use explicit pthread linkage:

```sh
gcc -std=c11 -O3 -Wall -Wextra -Wpedantic \
  -Wconversion -Wshadow -Werror \
  basaltc.c -lpthread -o basaltc.exe
```

The executable is a local build artifact and must remain outside the repository's tracked `tests/` tree. Keep temporary generated C and binaries under `.tmp/` or another ignored build directory.

## Generated-emitter workflow

The Windows portability support is now part of `emit_runtime` in `src/bootstrap/basaltc.basalt`. A normal Bootstrap translation automatically emits the conditional prologue, so no generated `basaltc.c` edit is required. The durable update workflow is:

```sh
# Build a current compiler from the checked-in seed.
source scripts/bootstrap_stage.sh
current_bin=$(bootstrap_stage "$PWD" .tmp/windows-bootstrap \
  -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)

# Regenerate the compiler C, then compile it in the target toolchain.
"$current_bin" src/bootstrap/basaltc.basalt .tmp/basaltc.generated.c
gcc -std=c11 -O3 -Wall -Wextra -Wpedantic \
  -Wconversion -Wshadow -Werror -pthread \
  .tmp/basaltc.generated.c -o .tmp/basaltc.exe

# After reviewing the generated output, synchronize the seed and checksum,
# then run the fixed-point and ownership-stress checks.
bash scripts/fixed_point.sh
bash scripts/run_ownership_stress.sh
```

The portability adapter is selected by compiler/platform feature detection rather than by blindly deleting `<threads.h>`. A native implementation remains available for Linux and macOS, while MinGW/UCRT64 selects the pthread adapter unless `BASALT_USE_NATIVE_C11_THREADS` is defined. The generated C also emits the guarded `void *aligned_alloc(size_t alignment, size_t size);` declaration on MinGW/UCRT64, after `<stddef.h>` has been included.

## References

[1]: https://packages.msys2.org/package/mingw-w64-ucrt-x86_64-winpthreads "MSYS2: mingw-w64-ucrt-x86_64-winpthreads package"
[2]: https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html "GCC: Options for Linking"
[3]: https://en.cppreference.com/w/c/memory/aligned_alloc "cppreference: aligned_alloc"
