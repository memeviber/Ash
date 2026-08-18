func add_3(a: int, b: int): int {
  return a + b + 3;
}

func main(): int {
  let f: fn(int, int): int = &add_3;
  let result: int = f(5, 9);
  if result != 17 then return 1;
  return 0;
}
