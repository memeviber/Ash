func main(): int {
  let a: int = (((0 << 3) | 6) >> 1);
  let b: int = (((0 - 2) | 33) - ((0 - 26) / 11));
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
