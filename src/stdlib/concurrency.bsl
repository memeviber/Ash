// C11 concurrency primitives exposed through a small, explicit Basalt API.
//
// Atomic operations use acquire/release or acq_rel ordering internally:
// - load: acquire
// - store: release
// - fetch_add and compare_exchange: acq_rel
//
// Channels are bounded, non-blocking SPSC channels. send/recv return:
//   1 = completed, 0 = temporarily full/empty, -1 = closed.
// Thread callbacks have signature fn(void*): int and are joined explicitly.
namespace concurrency {
  func atomic_make(initial: int): void* {
    return basalt_atomic_make(initial);
  }

  func atomic_load(value: void*): int {
    return basalt_atomic_load(value);
  }

  func atomic_store(value: void*, next: int): void {
    basalt_atomic_store(value, next);
  }

  func atomic_fetch_add(value: void*, delta: int): int {
    return basalt_atomic_fetch_add(value, delta);
  }

  func atomic_compare_exchange(value: void*, expected: int, desired: int): int {
    return basalt_atomic_compare_exchange(value, expected, desired);
  }

  func atomic_free(value: void*): void {
    basalt_atomic_free(value);
  }

  func channel_make(capacity: int): void* {
    return basalt_channel_make(capacity);
  }

  func channel_send(channel: void*, value: int): int {
    return basalt_channel_send(channel, value);
  }

  func channel_recv(channel: void*, output: int*): int {
    return basalt_channel_recv(channel, output);
  }

  func channel_close(channel: void*): void {
    basalt_channel_close(channel);
  }

  func channel_free(channel: void*): void {
    basalt_channel_free(channel);
  }

  func thread_spawn(entry: fn(void*): int, argument: void*): void* {
    return basalt_thread_spawn(entry, argument);
  }

  func thread_join(thread: void*): int {
    return basalt_thread_join(thread);
  }

  func thread_yield(): void {
    basalt_thread_yield();
  }
}
