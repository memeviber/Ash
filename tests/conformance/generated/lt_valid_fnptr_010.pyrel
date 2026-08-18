func add_10(a: int, b: int): int {
  return a + b + 10;
}

func main(): int {
  let f: fn(int, int): int = &add_10;
  let result: int = f(12, 23);
  if result != 45 then return 1;
  return 0;
}
