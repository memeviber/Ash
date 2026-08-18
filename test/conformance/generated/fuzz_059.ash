func main(): int {
  let a: int = ((((0 - 5) / 11) >> 2) << 3);
  let b: int = ((14 - 21) + (22 - 19));
  let c: int = (a + b);
  if c != 0 then {
    print c;
  } else {
    print 0;
  }
  return 0;
}
