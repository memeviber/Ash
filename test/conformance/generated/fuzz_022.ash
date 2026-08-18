func main(): int {
  let a: int = (((27 | 6) * (8 ^ 10)) / 1);
  let b: int = ((22 & 42) / 1);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
