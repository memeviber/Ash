include "../../../src/stdlib/map.bsl"
func main(): int {
  let m: map::HashMap<int, int> = map::new(0, 0);
  m = map::put(m, "wrong", 1);
  return 0;
}
