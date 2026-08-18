func add_14(a: int, b: int): int {
  return a + b + 14;
}

func main(): int {
  let f: fn(int, int): int = &add_14;
  let result: int = f(16, 31);
  if result != 61 then return 1;
  return 0;
}
