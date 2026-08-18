func add_13(a: int, b: int): int {
  return a + b + 13;
}

func main(): int {
  let f: fn(int, int): int = &add_13;
  let result: int = f(15, 29);
  if result != 57 then return 1;
  return 0;
}
