func add_7(a: int, b: int): int {
  return a + b + 7;
}

func main(): int {
  let f: fn(int, int): int = &add_7;
  let result: int = f(9, 17);
  if result != 33 then return 1;
  return 0;
}
