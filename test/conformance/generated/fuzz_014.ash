func main(): int {
  let a: int = ((((0 - 23) ^ 49) / 4) >> 0);
  let b: int = (((0 - 31) + 4) & 20);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
