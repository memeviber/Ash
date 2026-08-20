include "../../src/stdlib/concurrency.bsl"

func bad_worker(argument: void*): void {
}

func main(): int {
  let thread: void* = concurrency::thread_spawn(&bad_worker, null);
  return concurrency::thread_join(thread);
}
