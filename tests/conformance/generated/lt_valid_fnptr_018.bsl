func add_18(a: int, b: int): int {
  return a + b + 18;
}

func main(): int {
  let f: fn(int, int): int = &add_18;
  let result: int = f(20, 39);
  if result != 77 then return 1;
  return 0;
}
