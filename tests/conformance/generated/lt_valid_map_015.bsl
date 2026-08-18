include "../../../src/stdlib/map.bsl"
func main(): int {
  let m: map::HashMap<bool, int> = map::new(false, 0);
  m = map::put(m, true, 31);
  if map::length(m) != 1 then return 1;
  if map::contains_key(m, true) == false then return 2;
  m = map::remove(m, true);
  if map::length(m) != 0 then return 3;
  m = map::free(m);
  return 0;
}
