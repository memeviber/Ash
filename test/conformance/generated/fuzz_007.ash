func main(): int {
  let a: int = ((((0 - 31) << 2) / 3) | 38);
  let b: int = (((0 - 20) >> 3) << 3);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
