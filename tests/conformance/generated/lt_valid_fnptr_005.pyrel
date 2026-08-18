func add_5(a: int, b: int): int {
  return a + b + 5;
}

func main(): int {
  let f: fn(int, int): int = &add_5;
  let result: int = f(7, 13);
  if result != 25 then return 1;
  return 0;
}
