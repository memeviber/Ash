#ifndef BASALT_CONCURRENCY_RUNTIME_C
#define BASALT_CONCURRENCY_RUNTIME_C

#include <stdatomic.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct basalt_cancel_token {
  atomic_int requested;
} basalt_cancel_token;

void *basalt_cancel_make(void) {
  basalt_cancel_token *token = (basalt_cancel_token *)calloc(1, sizeof(*token));
  if (!token) return NULL;
  atomic_init(&token->requested, 0);
  return basalt_track(token);
}

int basalt_cancel_request(void *value) {
  basalt_cancel_token *token = (basalt_cancel_token *)value;
  if (!token || basalt_find(token) == (size_t)-1) return 1;
  atomic_store_explicit(&token->requested, 1, memory_order_release);
  return 0;
}

int basalt_cancelled(void *value) {
  basalt_cancel_token *token = (basalt_cancel_token *)value;
  if (!token || basalt_find(token) == (size_t)-1) return 1;
  return atomic_load_explicit(&token->requested, memory_order_acquire);
}

void basalt_cancel_free(void *value) {
  if (!value) return;
  basalt_release(value);
}

#if defined(_WIN32)
typedef struct basalt_mutex {
  CRITICAL_SECTION value;
} basalt_mutex;
#else
typedef struct basalt_mutex {
  pthread_mutex_t value;
} basalt_mutex;
#endif

void *basalt_mutex_make(void) {
  basalt_mutex *mutex = (basalt_mutex *)calloc(1, sizeof(*mutex));
  if (!mutex) return NULL;
#if defined(_WIN32)
  InitializeCriticalSection(&mutex->value);
#else
  if (pthread_mutex_init(&mutex->value, NULL) != 0) {
    free(mutex);
    return NULL;
  }
#endif
  return basalt_track(mutex);
}

int basalt_mutex_lock(void *value) {
  basalt_mutex *mutex = (basalt_mutex *)value;
  if (!mutex || basalt_find(mutex) == (size_t)-1) return 1;
#if defined(_WIN32)
  EnterCriticalSection(&mutex->value);
  return 0;
#else
  return pthread_mutex_lock(&mutex->value) == 0 ? 0 : 1;
#endif
}

int basalt_mutex_unlock(void *value) {
  basalt_mutex *mutex = (basalt_mutex *)value;
  if (!mutex || basalt_find(mutex) == (size_t)-1) return 1;
#if defined(_WIN32)
  LeaveCriticalSection(&mutex->value);
  return 0;
#else
  return pthread_mutex_unlock(&mutex->value) == 0 ? 0 : 1;
#endif
}

int basalt_mutex_free(void *value) {
  basalt_mutex *mutex = (basalt_mutex *)value;
  int status;
  if (!mutex || basalt_find(mutex) == (size_t)-1) return 1;
#if defined(_WIN32)
  DeleteCriticalSection(&mutex->value);
  basalt_release(mutex);
  return 0;
#else
  status = pthread_mutex_destroy(&mutex->value);
  if (status != 0) return 1;
  basalt_release(mutex);
  return 0;
#endif
}

#endif
