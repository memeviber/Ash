func add_22(a: int, b: int): int {
  return a + b + 22;
}

func main(): int {
  let f: fn(int, int): int = &add_22;
  let result: int = f(24, 47);
  if result != 93 then return 1;
  return 0;
}
