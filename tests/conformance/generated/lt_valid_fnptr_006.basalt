func add_6(a: int, b: int): int {
  return a + b + 6;
}

func main(): int {
  let f: fn(int, int): int = &add_6;
  let result: int = f(8, 15);
  if result != 29 then return 1;
  return 0;
}
