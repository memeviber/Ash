func main(): int {
  let a: int = ((((31 - 0) << 2) / 3) | 38);
  let b: int = (((20 - 0) >> 3) << 3);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
