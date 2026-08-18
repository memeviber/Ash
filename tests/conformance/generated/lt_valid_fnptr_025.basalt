func add_25(a: int, b: int): int {
  return a + b + 25;
}

func main(): int {
  let f: fn(int, int): int = &add_25;
  let result: int = f(27, 53);
  if result != 105 then return 1;
  return 0;
}
