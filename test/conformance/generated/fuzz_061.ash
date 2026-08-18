func main(): int {
  let a: int = (((4 + (0 - 8)) / 5) >> 0);
  let b: int = (((0 - 23) << 3) << 1);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
