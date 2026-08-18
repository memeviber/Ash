include "../../src/stdlib/map.ash"

func main(): int {
  let map: map::IntMap = map::new_int(4);
  map = map::put_int(map, 7, 70);
  map = map::put_int(map, 9, 90);
  print map::get_int(map, 7, (0 - 1));
  print map::get_int(map, 8, (0 - 1));
  map::free_int(map);
  return 0;
}
