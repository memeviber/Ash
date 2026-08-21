# Windows UCRT64 Bootstrap Portability

This guide explains how to compile the Bootstrap compiler's generated C on Windows from an **MSYS2 UCRT64** shell. The Bootstrap runtime currently emits C11 thread calls such as `thrd_create`, `thrd_join`, and `thrd_yield`, while some MinGW/UCRT64 installations provide the underlying POSIX-compatible `pthread` interface rather than a usable `<threads.h>` header.

The repository's source of truth remains `src/bootstrap/basaltc.bsl`. Editing a generated `basaltc.c` or `basaltc.seed.c` is a temporary portability experiment only; the durable fix belongs in the Bootstrap runtime-prologue emitter and must then be regenerated through the seed/fixed-point workflow.

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

The generated runtime currently includes `<threads.h>`. If the selected UCRT64 headers do not expose that C11 header, compilation fails before the linker is reached. The installed winpthreads package supplies a POSIX-compatible thread layer, so the generated runtime can use a small adapter that preserves Basalt's C11-shaped calls while delegating to `pthread_create`, `pthread_join`, and `sched_yield`.

GCC documents `-pthread` as the thread-support option and explicitly lists MinGW targets among the supported environments [2]. Prefer `-pthread` for both compilation and linking. An explicit `-lpthread` link option is also shown below for toolchains that require it, but it does not always provide the same preprocessor configuration as `-pthread`.

## Safe pthread compatibility shim

Place this block in the generated C runtime prologue **before** the Basalt runtime uses `thrd_t` or any `thrd_*` function. It is safer than the frequently copied one-line macros because `pthread_create` expects a `void *(*)(void *)` entry point, whereas a C11 thread entry point returns `int`. It also avoids casting an `int *` to `void **` for `pthread_join`, which can overwrite more bytes than the destination integer on 64-bit Windows.

```c
#if defined(__MINGW32__) && !defined(BASALT_USE_NATIVE_C11_THREADS)
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>

typedef pthread_t thrd_t;
typedef int (*thrd_start_t)(void *);
#define thrd_success 0

typedef struct basalt_thrd_start_context {
    thrd_start_t entry;
    void *argument;
    int result;
} basalt_thrd_start_context;

static void *basalt_thrd_start(void *raw) {
    basalt_thrd_start_context *context = (basalt_thrd_start_context *)raw;
    context->result = context->entry(context->argument);
    return raw;
}

static int basalt_thrd_create(thrd_t *thread, thrd_start_t entry, void *argument) {
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

## Recommended long-term fix

The manual generated-C patch is useful for confirming that the problem is limited to platform headers and thread ABI adaptation. For a permanent cross-platform release, add the conditional prologue to the Bootstrap emitter in `src/bootstrap/basaltc.bsl`, mirror the same prologue in the supported reference path when parity is required, regenerate the Bootstrap C seed, update `fixed_point_production.sha256`, and run `scripts/run_ownership_stress.sh` before committing.

The portability adapter should be selected by compiler/platform feature detection rather than by blindly deleting `<threads.h>`. A native implementation should remain available for Linux and macOS, while UCRT64 should select the pthread adapter only when the native C11 header is unavailable or explicitly disabled.

## References

[1]: https://packages.msys2.org/package/mingw-w64-ucrt-x86_64-winpthreads "MSYS2: mingw-w64-ucrt-x86_64-winpthreads package"
[2]: https://gcc.gnu.org/onlinedocs/gcc/Link-Options.html "GCC: Options for Linking"
[3]: https://en.cppreference.com/w/c/memory/aligned_alloc "cppreference: aligned_alloc"
