func add_31(a: int, b: int): int {
  return a + b + 31;
}

func main(): int {
  let f: fn(int, int): int = &add_31;
  let result: int = f(33, 65);
  if result != 129 then return 1;
  return 0;
}
