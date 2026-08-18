func add_23(a: int, b: int): int {
  return a + b + 23;
}

func main(): int {
  let f: fn(int, int): int = &add_23;
  let result: int = f(25, 49);
  if result != 97 then return 1;
  return 0;
}
