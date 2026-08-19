include "../../src/stdlib/map.bsl"

func spread_hash(value: int): int {
  return value * 31;
}

func same_int(a: int, b: int): bool {
  return a == b;
}

func main(): int {
  let m: map::HashMap<int, int> = map::new_with_hasher(0, 0, &spread_hash, &same_int);
  let i: int = 0;
  while i < 2048 {
    m = map::put(m, i, i * 3);
    i = i + 1;
  }
  if map::length(m) != 2048 then return 1;
  if map::capacity(m) < 4096 then return 2;
  if (map::capacity(m) & (map::capacity(m) - 1)) != 0 then return 3;

  i = 0;
  while i < 2048 {
    if i % 3 == 0 then m = map::remove(m, i);
    i = i + 1;
  }
  if map::length(m) != 1365 then return 4;
  if map::contains_key(m, 0) then return 5;
  if map::contains_key(m, 2047) == false then return 6;

  i = 0;
  while i < 2048 {
    if i % 3 == 0 then m = map::put(m, i, i * 7);
    i = i + 1;
  }
  if map::length(m) != 2048 then return 7;
  if map::get_or(m, 0, (0 - 1)) != 0 then return 8;
  if map::get_or(m, 1023, (0 - 1)) != 7161 then return 9;
  if map::get_or(m, 2047, (0 - 1)) != 6141 then return 10;

  m = map::clear(m);
  if map::length(m) != 0 then return 11;
  i = 0;
  while i < 512 {
    m = map::put(m, i, 100000 - i);
    i = i + 1;
  }
  if map::get_or(m, 511, (0 - 1)) != 99489 then return 12;
  m = map::free(m);
  return 0;
}
