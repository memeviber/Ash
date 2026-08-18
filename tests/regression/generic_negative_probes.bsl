include "../../src/stdlib/map.bsl"
include "../../src/stdlib/result.bsl"

func main(): int {
  let m: map::HashMap<int, int> = map::new(0, 0);
  m = map::put(m, "wrong-key", 42);
  return 0;
}

struct BadArity<T, U> {
  first: T;
  second: U;
}

func bad_arity(): BadArity<int> {
  let x: BadArity<int> = 0;
  return x;
}
