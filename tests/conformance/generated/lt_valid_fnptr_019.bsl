func add_19(a: int, b: int): int {
  return a + b + 19;
}

func main(): int {
  let f: fn(int, int): int = &add_19;
  let result: int = f(21, 41);
  if result != 81 then return 1;
  return 0;
}
