func add_11(a: int, b: int): int {
  return a + b + 11;
}

func main(): int {
  let f: fn(int, int): int = &add_11;
  let result: int = f(13, 25);
  if result != 49 then return 1;
  return 0;
}
