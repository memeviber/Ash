func main(): int {
  let a: int = (((7 + (0 - 10)) << 2) & 61);
  let b: int = (((0 - 6) * 7) << 2);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
