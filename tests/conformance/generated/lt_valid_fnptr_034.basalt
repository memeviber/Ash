func add_34(a: int, b: int): int {
  return a + b + 34;
}

func main(): int {
  let f: fn(int, int): int = &add_34;
  let result: int = f(36, 71);
  if result != 141 then return 1;
  return 0;
}
