func add_12(a: int, b: int): int {
  return a + b + 12;
}

func main(): int {
  let f: fn(int, int): int = &add_12;
  let result: int = f(14, 27);
  if result != 53 then return 1;
  return 0;
}
