func add_20(a: int, b: int): int {
  return a + b + 20;
}

func main(): int {
  let f: fn(int, int): int = &add_20;
  let result: int = f(22, 43);
  if result != 85 then return 1;
  return 0;
}
