include "../../src/stdlib/map.bsl"

func main(): int {
  let m: map::HashMap<int, int> = map::new(0, 0);
  let bad: map::HashMap<int, int> = map::put(m, "wrong-key", 42);
  return bad.len;
}
