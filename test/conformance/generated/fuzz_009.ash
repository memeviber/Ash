func main(): int {
  let a: int = (((21 & 61) + 6) * (((0 - 6) >> 3) << 2));
  let b: int = ((12 ^ 42) / 2);
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
