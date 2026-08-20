include "../../src/stdlib/concurrency.bsl"

func main(): int {
  return concurrency::atomic_load(0);
}
