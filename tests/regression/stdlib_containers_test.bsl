include "../../src/stdlib/string.bsl"
include "../../src/stdlib/slice.bsl"
include "../../src/stdlib/map.bsl"

func main(): int {
  let text: string = "Aé";
  print str::byte_len(text);
  print str::byte_at(text, 0);
  print str::eq(text, "Aé");
  print str::eq(text, "Ax");

  let slice: slice::Slice<int> = slice::new(0);
  slice = slice::push(slice, 10);
  slice = slice::push(slice, 20);
  slice = slice::push(slice, 30);
  print slice.len;
  print slice::get(slice, 0);
  print slice::get(slice, 2);
  print slice::get(slice, 9);
  slice = slice::free(slice);

  let map: map::HashMap<int, int> = map::new(0, 0);
  map = map::put(map, 7, 70);
  map = map::put(map, 9, 90);
  print map::get_or(map, 7, (0 - 1));
  print map::get_or(map, 8, (0 - 1));
  map = map::free(map);
  return 0;
}
