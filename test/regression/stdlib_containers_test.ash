include "../../src/stdlib/string.ash"
include "../../src/stdlib/slice.ash"
include "../../src/stdlib/map.ash"

func main(): int {
  let text: string = "Aé";
  print string_byte_len(text);
  print string_at(text, 0);
  print string_eq(text, "Aé");
  print string_eq(text, "Ax");

  let slice: slice::IntSlice = slice::new_int(2);
  slice = slice::push_int(slice, 10);
  slice = slice::push_int(slice, 20);
  slice = slice::push_int(slice, 30);
  print slice.len;
  print slice::get_int(slice, 0);
  print slice::get_int(slice, 2);
  print slice::get_int(slice, 9);
  slice::free_int(slice);

  let map: map::IntMap = map::new_int(4);
  map = map::put_int(map, 7, 70);
  map = map::put_int(map, 9, 90);
  print map::get_int(map, 7, (0 - 1));
  print map::get_int(map, 8, (0 - 1));
  map::free_int(map);
  return 0;
}
