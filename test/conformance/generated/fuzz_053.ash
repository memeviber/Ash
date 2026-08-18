func main(): int {
  let a: int = (((12 & 4) ^ 24) / 5);
  let b: int = (((0 - 23) + 4) | 62);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
