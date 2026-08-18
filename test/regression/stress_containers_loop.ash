include "../../src/stdlib/string.ash"
include "../../src/stdlib/slice.ash"
include "../../src/stdlib/map.ash"

func main(): int {
  let round: int = 0;
  while (round < 120) {
    let s: string = "Ash";
    let k: int = 0;
    while (k < 24) {
      s = string_concat(s, "-x");
      k = k + 1;
    }
    if (string_byte_len(s) == 51) then {
      print 1;
    } else {
      print 0;
    }

    let slice: slice::IntSlice = slice::new_int(1);
    let i: int = 0;
    while (i < 32) {
      slice = slice::push_int(slice, round + i);
      i = i + 1;
    }
    print slice::get_int(slice, 31);
    print slice::get_int(slice, 32);
    slice::free_int(slice);

    let map: map::IntMap = map::new_int(2);
    let m: int = 0;
    while (m < 40) {
      map = map::put_int(map, round + m, m * 3);
      m = m + 1;
    }
    print map::get_int(map, round + 17, (0 - 1));
    print map::get_int(map, 99999, (0 - 1));
    map::free_int(map);
    round = round + 1;
  }
  return 0;
}
