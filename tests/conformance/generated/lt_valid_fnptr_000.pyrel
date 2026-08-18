func add_0(a: int, b: int): int {
  return a + b + 0;
}

func main(): int {
  let f: fn(int, int): int = &add_0;
  let result: int = f(2, 3);
  if result != 5 then return 1;
  return 0;
}
