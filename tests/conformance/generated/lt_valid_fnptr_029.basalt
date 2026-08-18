func add_29(a: int, b: int): int {
  return a + b + 29;
}

func main(): int {
  let f: fn(int, int): int = &add_29;
  let result: int = f(31, 61);
  if result != 121 then return 1;
  return 0;
}
