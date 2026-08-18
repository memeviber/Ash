func add_24(a: int, b: int): int {
  return a + b + 24;
}

func main(): int {
  let f: fn(int, int): int = &add_24;
  let result: int = f(26, 51);
  if result != 101 then return 1;
  return 0;
}
