func add_2(a: int, b: int): int {
  return a + b + 2;
}

func main(): int {
  let f: fn(int, int): int = &add_2;
  let result: int = f(4, 7);
  if result != 13 then return 1;
  return 0;
}
