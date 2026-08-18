func main(): int {
  let a: int = (((4 * (0 - 3)) + 4) << 1);
  let b: int = (((0 - 26) >> 1) << 3);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
