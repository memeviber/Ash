func add_21(a: int, b: int): int {
  return a + b + 21;
}

func main(): int {
  let f: fn(int, int): int = &add_21;
  let result: int = f(23, 45);
  if result != 89 then return 1;
  return 0;
}
