struct Pair {
  data: int*;
  cap: int;
}

extern func alloc_ints(size: int): int*;
extern func grow_ints(p: int*, old_cap: int, new_cap: int): int*;

func update(p: Pair): Pair {
  if p.cap >= 2 then {
    p.data = grow_ints(p.data, p.cap, p.cap + p.cap);
    p.cap = p.cap + p.cap;
  }
  return p;
}

func main(): int {
  let p: Pair = 0;
  p.data = alloc_ints(2);
  p.cap = 2;
  p = update(p);
  return 0;
}
