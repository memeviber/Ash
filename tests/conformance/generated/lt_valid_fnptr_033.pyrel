func add_33(a: int, b: int): int {
  return a + b + 33;
}

func main(): int {
  let f: fn(int, int): int = &add_33;
  let result: int = f(35, 69);
  if result != 137 then return 1;
  return 0;
}
