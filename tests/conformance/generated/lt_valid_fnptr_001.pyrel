func add_1(a: int, b: int): int {
  return a + b + 1;
}

func main(): int {
  let f: fn(int, int): int = &add_1;
  let result: int = f(3, 5);
  if result != 9 then return 1;
  return 0;
}
