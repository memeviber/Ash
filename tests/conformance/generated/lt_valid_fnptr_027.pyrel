func add_27(a: int, b: int): int {
  return a + b + 27;
}

func main(): int {
  let f: fn(int, int): int = &add_27;
  let result: int = f(29, 57);
  if result != 113 then return 1;
  return 0;
}
