func add_4(a: int, b: int): int {
  return a + b + 4;
}

func main(): int {
  let f: fn(int, int): int = &add_4;
  let result: int = f(6, 11);
  if result != 21 then return 1;
  return 0;
}
