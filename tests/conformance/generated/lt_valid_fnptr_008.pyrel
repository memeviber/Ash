func add_8(a: int, b: int): int {
  return a + b + 8;
}

func main(): int {
  let f: fn(int, int): int = &add_8;
  let result: int = f(10, 19);
  if result != 37 then return 1;
  return 0;
}
