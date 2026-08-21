func add_28(a: int, b: int): int {
  return a + b + 28;
}

func main(): int {
  let f: fn(int, int): int = &add_28;
  let result: int = f(30, 59);
  if result != 117 then return 1;
  return 0;
}
