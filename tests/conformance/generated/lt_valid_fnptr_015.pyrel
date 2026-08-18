func add_15(a: int, b: int): int {
  return a + b + 15;
}

func main(): int {
  let f: fn(int, int): int = &add_15;
  let result: int = f(17, 33);
  if result != 65 then return 1;
  return 0;
}
