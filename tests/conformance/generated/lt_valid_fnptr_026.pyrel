func add_26(a: int, b: int): int {
  return a + b + 26;
}

func main(): int {
  let f: fn(int, int): int = &add_26;
  let result: int = f(28, 55);
  if result != 109 then return 1;
  return 0;
}
