func add_9(a: int, b: int): int {
  return a + b + 9;
}

func main(): int {
  let f: fn(int, int): int = &add_9;
  let result: int = f(11, 21);
  if result != 41 then return 1;
  return 0;
}
