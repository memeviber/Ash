func add_17(a: int, b: int): int {
  return a + b + 17;
}

func main(): int {
  let f: fn(int, int): int = &add_17;
  let result: int = f(19, 37);
  if result != 73 then return 1;
  return 0;
}
