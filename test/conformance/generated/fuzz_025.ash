func main(): int {
  let a: int = (((1 & 55) | 41) - ((10 & 37) | 6));
  let b: int = (((0 - 3) & 28) << 1);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
