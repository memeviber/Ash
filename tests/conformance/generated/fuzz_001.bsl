func main(): int {
  let a: int = (((7 + (10 - 0)) << 2) & 61);
  let b: int = (((6 - 0) * 7) << 2);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
