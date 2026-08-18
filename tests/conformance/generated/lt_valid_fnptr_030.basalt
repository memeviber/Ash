func add_30(a: int, b: int): int {
  return a + b + 30;
}

func main(): int {
  let f: fn(int, int): int = &add_30;
  let result: int = f(32, 63);
  if result != 125 then return 1;
  return 0;
}
