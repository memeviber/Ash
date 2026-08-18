func main(): int {
  let a: int = ((((0 - 5) / 7) << 0) >> 0);
  let b: int = (((0 - 1) | 0) + 3);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
