func main(): int {
  let a: int = (((5 ^ 49) | 17) >> 1);
  let b: int = (((0 - 12) - 4) | 44);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
