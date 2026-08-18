func add(a: int, b: int): int {
  return a + b;
}
func main(): void {
  let f: fn(int, int): int = &add;
  let x: int = f(2, 3);
  print x;
}
