func half(x: int): double { return x / 2.0; }
func main(): int {
  let f: fn(int): double = &half;
  let v: double = f(5);
  print v;
  return 0;
}
