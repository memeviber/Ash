func add_32(a: int, b: int): int {
  return a + b + 32;
}

func main(): int {
  let f: fn(int, int): int = &add_32;
  let result: int = f(34, 67);
  if result != 133 then return 1;
  return 0;
}
